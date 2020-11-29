/***********************************************************************
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

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

// Qt
#include <QCommandLineParser>

/* utility */
#include "fciconv.h"
#include "fcintl.h"
#include "support.h"

/* common */
#include "version.h"

/* modinst */
#include "modinst.h"

#include "mpcmdline.h"

extern struct fcmp_params fcmp;

/**********************************************************************/ /**
   Parse commandline parameters. Modified argv[] so it contains only
   ui-specific options afterwards. Number of those ui-specific options is
   returned.
   Currently this is implemented in a way that it never fails. Either it
   returns with success or exit()s. Implementation can be changed so that
   this returns with value -1 in case program should be shut down instead
   of exiting itself. Callers are prepared for such implementation.
   This call initialises the log system.
 **************************************************************************/
void fcmp_parse_cmdline(const QCoreApplication &app)
{
  QCommandLineParser parser;
  parser.addHelpOption();
  parser.addVersionOption();

  bool ok = parser.addOptions(
      {{{"d", _("debug")},
        // TRANS: Do not translate "fatal", "critical", "warning", "info" or
        //        "debug". It's exactly what the user must type.
        _("Set debug log level (fatal/critical/warning/info/debug)"),
        _("LEVEL"),
        QStringLiteral("info")},
       {{"i", "install"},
        _("Automatically install modpack from a given URL"),
        // TRANS: Command line argument
        _("URL")},
       {{"L", "List"},
        _("Load modpack list from given URL"),
        // TRANS: Command line argument
        _("URL")},
       {{"p", "prefix"},
        _("Install modpacks to given directory hierarchy"),
        // TRANS: Command line argument
        _("DIR")}});
  if (!ok) {
    qFatal("Adding command line arguments failed");
    exit(EXIT_FAILURE);
  }

  // Parse
  parser.process(app);

  // Process the parsed options
  if (log_init(parser.value(QStringLiteral("debug")))) {
    exit(EXIT_FAILURE);
  }
  if (parser.isSet(QStringLiteral("List"))) {
    fcmp.list_url =
        QUrl::fromUserInput(parser.value(QStringLiteral("List")));
  }
  if (parser.isSet(QStringLiteral("prefix"))) {
    fcmp.inst_prefix = parser.value(QStringLiteral("prefix"));
  }
  if (parser.isSet(QStringLiteral("install"))) {
    fcmp.autoinstall = parser.value(QStringLiteral("install"));
  }

  if (fcmp.inst_prefix.isNull()) {
    fcmp.inst_prefix = freeciv_storage_dir();

    if (fcmp.inst_prefix.isNull()) {
      qCritical("Cannot determine freeciv storage directory");
    }
  }
}