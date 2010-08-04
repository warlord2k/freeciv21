/********************************************************************** 
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/
#ifndef FC__AI_H
#define FC__AI_H

#define AI_DEFAULT 1

struct Treaty;
struct player;
struct ai_choice;
struct city;
struct unit;

enum incident_type {
  INCIDENT_DIPLOMAT = 0, INCIDENT_WAR, INCIDENT_PILLAGE,
  INCIDENT_NUCLEAR, INCIDENT_NUCLEAR_NOT_TARGET,
  INCIDENT_NUCLEAR_SELF, INCIDENT_LAST
};

struct ai_type
{
  struct {
    void (*data_init)(struct player *pplayer);
    void (*data_default)(struct player *pplayer);
    void (*data_close)(struct player *pplayer);

    void (*city_init)(struct city *pcity);
    void (*city_update)(struct city *pcity);
    void (*city_close)(struct city *pcity);

    void (*unit_init)(struct unit *punit);
    void (*unit_turn_end)(struct unit *punit);
    void (*unit_close)(struct unit *punit);

    void (*first_activities)(struct player *pplayer);
    void (*diplomacy_actions)(struct player *pplayer);
    void (*last_activities)(struct player *pplayer);
    void (*before_auto_settlers)(struct player *pplayer);
    void (*treaty_evaluate)(struct player *pplayer, struct player *aplayer, struct Treaty *ptreaty);
    void (*treaty_accepted)(struct player *pplayer, struct player *aplayer, struct Treaty *ptreaty);
    void (*first_contact)(struct player *pplayer, struct player *aplayer);
    void (*incident)(enum incident_type type, struct player *violator,
                     struct player *victim);
  } funcs;
};

struct ai_type *get_ai_type(int id);
void init_ai(struct ai_type *ai);


#define ai_type_iterate(NAME_ai) \
do { \
  struct ai_type *NAME_ai = get_ai_type(AI_DEFAULT);

#define ai_type_iterate_end \
  } while (FALSE);

#endif  /* FC__AI_H */
