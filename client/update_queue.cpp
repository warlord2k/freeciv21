/*__            ___                 ***************************************
/   \          /   \          Copyright (c) 1996-2020 Freeciv21 and Freeciv
\_   \        /  __/          contributors. This file is part of Freeciv21.
 _\   \      /  /__     Freeciv21 is free software: you can redistribute it
 \___  \____/   __/    and/or modify it under the terms of the GNU  General
     \_       _/          Public License  as published by the Free Software
       | @ @  \_               Foundation, either version 3 of the  License,
       |                              or (at your option) any later version.
     _/     /\                  You should have received  a copy of the GNU
    /o)  (o/\ \_                General Public License along with Freeciv21.
    \_____/ /                     If not, see https://www.gnu.org/licenses/.
      \____/        ********************************************************/

// Qt
#include <QUrl>

// utility
#include "log.h"
#include "support.h" // bool

// common
#include "city.h"
#include "player.h"

/* client/include */
#include "citydlg_g.h"
#include "cityrep_g.h"
#include "dialogs_g.h"
#include "gui_main_g.h"
#include "menu_g.h"
#include "pages_g.h"
#include "ratesdlg_g.h"
#include "repodlgs_g.h"

// client
#include "client_main.h"
#include "connectdlg_common.h"
#include "plrdlg_common.h"
#include "tilespec.h"
#include "update_queue.h"

// gui-qt
#include "canvas.h"
#include "qtg_cxxside.h"

update_queue *update_queue::m_instance = nullptr;

// returns instance of queue
update_queue *update_queue::uq()
{
  if (!m_instance)
    m_instance = new update_queue;
  return m_instance;
}

void update_queue::drop()
{
  delete m_instance;
  m_instance = nullptr;
}

// Moves the instances waiting to the request_id to the callback queue.
void update_queue::wq_run_requests(int request_id)
{
  if (!wq_processing_finished.contains(request_id)) {
    return;
  }

  auto list = wq_processing_finished.value(request_id);
  for (const auto &wq_data : qAsConst(list)) {
    queue.push_back(wq_data);
  }
  wq_processing_finished.remove(request_id);
}

// Free a waiting queue data.
void update_queue::wq_data_destroy(waiting_queue_data &wq_data)
{
  if (wq_data.data && wq_data.free_data_func) {
    // May be nullptr, see waiting_queue_data_extract().
    wq_data.free_data_func(wq_data.data);
  }
  wq_data.data = nullptr;
  wq_data.free_data_func = nullptr;
}

// Connects the callback to a network event.
void update_queue::wq_add_request(int request_id, uq_callback_t callback,
                                  void *data, uq_free_fn_t free_data_func)
{
  wq_processing_finished[request_id].append(
      {callback, data, free_data_func});
}

// clears content
void update_queue::init()
{
  while (!queue.isEmpty()) {
    auto wq = queue.dequeue();
    wq_data_destroy(wq);
  }

  for (auto a : qAsConst(wq_processing_finished)) {
    for (auto &data : a) {
      wq_data_destroy(data);
    }
  }
  wq_processing_finished.clear();
}

update_queue::~update_queue() { init(); }

// Moves the instances waiting to the request_id to the callback queue.
void update_queue::processing_finished(int request_id)
{
  wq_run_requests(request_id);
}

// Unqueue all updates.
void update_queue::update_unqueue()
{
  has_idle_cb = false;

  // Invoke callbacks.
  while (!queue.isEmpty()) {
    auto wq = queue.dequeue();
    wq.callback(wq.data);
    wq_data_destroy(wq);
  }
}

// Add a callback to the update queue. NB: you can only set a callback
// once. Setting a callback twice will put new callback at end.
void update_queue::push(const waiting_queue_data &wq)
{
  queue.removeAll(wq);
  queue.enqueue(wq);

  if (!has_idle_cb) {
    has_idle_cb = true;
    update_unqueue();
  }
}

// Add a callback to the update queue. NB: you can only set a callback
// once. Setting a callback twice will overwrite the previous.
void update_queue::add(uq_callback_t callback)
{
  push({callback, nullptr, nullptr});
}

// Returns whether this callback is listed in the update queue.
bool update_queue::has_callback(uq_callback_t callback)
{
  return std::any_of(queue.cbegin(), queue.cend(),
                     [&](auto &wq) { return wq.callback == callback; });
}

// Connects the callback to the end of the processing (in server side) of
// the request.
void update_queue::connect_processing_finished(int request_id,
                                               uq_callback_t callback,
                                               void *data)
{
  wq_add_request(request_id, callback, data, nullptr);
}

/**
 * Connects the callback to the end of the processing (in server side) of
 * the request. The callback will be called only once for this request.
 */
void update_queue::connect_processing_finished_unique(int request_id,
                                                      uq_callback_t callback,
                                                      void *data)
{
  if (wq_processing_finished.contains(request_id)) {
    for (const auto &d : wq_processing_finished[request_id]) {
      if (d.callback == callback && d.data == data) {
        // Already present
        return;
      }
    }
  }
  wq_add_request(request_id, callback, data, nullptr);
}

// Connects the callback to the end of the processing (in server side) of
// the request.
void update_queue::connect_processing_finished_full(
    int request_id, uq_callback_t callback, void *data,
    uq_free_fn_t free_data_func)
{
  wq_add_request(request_id, callback, data, free_data_func);
}

namespace {
enum client_pages next_client_page = PAGE_MAIN;
}

// Set the client page.
static void set_client_page_callback(void *)
{
  real_set_client_page(next_client_page);
}

// Set the client page.
void set_client_page(enum client_pages page)
{
  log_debug("Requested page: %s.", client_pages_name(page));

  next_client_page = page;
  update_queue::uq()->add(set_client_page_callback);
}

// Start server and then, set the client page.
void client_start_server_and_set_page(enum client_pages page)
{
  log_debug("Requested server start + page: %s.", client_pages_name(page));

  if (client_start_server(client_url().userName())) {
    next_client_page = page;
    update_queue::uq()->connect_processing_finished(
        client.conn.client.last_request_id_used, set_client_page_callback,
        nullptr);
  }
}

// Returns the next client page.
enum client_pages get_client_page(void)
{
  if (update_queue::uq()->has_callback(set_client_page_callback)) {
    return next_client_page;
  } else {
    return get_current_client_page();
  }
}

// Returns whether there's page switching already in progress.
bool update_queue_is_switching_page(void)
{
  return update_queue::uq()->has_callback(set_client_page_callback);
}

// Request the menus to be initialized and updated.
void menus_init(void)
{
  update_queue::uq()->add([](void *) {
    real_menus_init();
    real_menus_update();
  });
}

// Update the menus.
static void menus_update_callback(void *) { real_menus_update(); }

// Request the menus to be updated.
void menus_update(void)
{
  if (!update_queue::uq()->has_callback(menus_update_callback)) {
    update_queue::uq()->add(menus_update_callback);
  }
}

// Update multipliers/policy dialog.
void multipliers_dialog_update(void)
{
  update_queue::uq()->add(real_multipliers_dialog_update);
}

// Update cities gui.
static void cities_update_callback(void *data)
{
#ifdef FREECIV_DEBUG
#define NEED_UPDATE(city_update, action)                                    \
  if (city_update & need_update) {                                          \
    action;                                                                 \
    need_update = static_cast<city_updates>(need_update & ~city_update);    \
  }
#else // FREECIV_DEBUG
#define NEED_UPDATE(city_update, action)                                    \
  if (city_update & need_update) {                                          \
    action;                                                                 \
  }
#endif // FREECIV_DEBUG

  cities_iterate(pcity)
  {
    enum city_updates need_update = pcity->client.need_updates;

    if (CU_NO_UPDATE == need_update) {
      continue;
    }

    // Clear all updates.
    pcity->client.need_updates = CU_NO_UPDATE;

    NEED_UPDATE(CU_UPDATE_REPORT, real_city_report_update_city(pcity));
    NEED_UPDATE(CU_UPDATE_DIALOG, real_city_dialog_refresh(pcity));
    NEED_UPDATE(CU_POPUP_DIALOG, real_city_dialog_popup(pcity));

#ifdef FREECIV_DEBUG
    if (CU_NO_UPDATE != need_update) {
      qCritical("Some city updates not handled "
                "for city %s (id %d): %d left.",
                city_name_get(pcity), pcity->id, need_update);
    }
#endif // FREECIV_DEBUG
  }
  cities_iterate_end;
#undef NEED_UPDATE
}

// Request the city dialog to be popped up for the city.
void popup_city_dialog(struct city *pcity)
{
  pcity->client.need_updates =
      static_cast<city_updates>(static_cast<int>(pcity->client.need_updates)
                                | static_cast<int>(CU_POPUP_DIALOG));
  update_queue::uq()->add(cities_update_callback);
}

// Request the city dialog to be updated for the city.
void refresh_city_dialog(struct city *pcity)
{
  pcity->client.need_updates =
      static_cast<city_updates>(static_cast<int>(pcity->client.need_updates)
                                | static_cast<int>(CU_UPDATE_DIALOG));
  update_queue::uq()->add(cities_update_callback);
}

// Request the city to be updated in the city report.
void city_report_dialog_update_city(struct city *pcity)
{
  pcity->client.need_updates =
      static_cast<city_updates>(static_cast<int>(pcity->client.need_updates)
                                | static_cast<int>(CU_UPDATE_REPORT));
  update_queue::uq()->add(cities_update_callback);
}

// Update the connection list in the start page.
void conn_list_dialog_update(void)
{
  update_queue::uq()->add(real_conn_list_dialog_update);
}

// Update the nation report.
void players_dialog_update(void)
{
  update_queue::uq()->add(real_players_dialog_update);
}

// Update the city report.
void city_report_dialog_update(void)
{
  update_queue::uq()->add(real_city_report_dialog_update);
}

// Update the science report.
void science_report_dialog_update(void)
{
  update_queue::uq()->add(real_science_report_dialog_update);
}

// Update the economy report.
void economy_report_dialog_update(void)
{
  update_queue::uq()->add(real_economy_report_dialog_update);
}

// Update the units report.
void units_report_dialog_update(void)
{
  update_queue::uq()->add(real_units_report_dialog_update);
}

// Update the units report.
void unit_select_dialog_update(void)
{
  update_queue::uq()->add(unit_select_dialog_update_real);
}
