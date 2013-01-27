/***************************************************************************
 *   Copyright (C) 2013 by Paul Ryvkin                                     *
 *   paulryvkin@paulryvkin.com                                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; version 2 of the License.               *

 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <string>
#include <iostream>

using namespace std;

#include "keepassx.h"

#include "Kdb3Database.h"
#include "IIconTheme.h"

#include "merge_db.h"

KpxConfig *config;
QString  AppDir;
QString HomeDir;
QString DataDir;

QPixmap* EntryIcons;
IIconTheme* IconLoader=NULL;

int main(int argc, char **argv) {
  if (argc < 5) {
    cerr << "USAGE: " << argv[0]
	 << " merge file1.kdb file2.kdb out.kdb\n";
    return(1);
  }
  string cmd(argv[1]);
  string filename1(argv[2]);
  string filename2(argv[3]);
  string filenameout(argv[4]);

  if (cmd != "merge") {
    cerr << "Invalid command: " << cmd << "\n";
    return(1);
  }

  initYarrow(); //init random number generator
  SecString::generateSessionKey();
  QString IniFilename="config.ini";
  config = new KpxConfig(IniFilename);

  //////

  Kdb3Database *db1 = new Kdb3Database();
  Kdb3Database *db2 = new Kdb3Database();

  string pw(getpass("Password: "));

  db1->setKey(pw.c_str(), "");
  if (!db1->load(QString(filename1.c_str()), true)) {
    cerr << "Could not load file 1: " << filename1 << "\n";
    return(1);
  }
  db2->setKey(pw.c_str(), "");
  if (!db2->load(QString(filename2.c_str()), true)) {
    cerr << "Could not load file 2: " << filename2 << "\n";
    return(1);
  }

  merge_dbs(db1, db2);

  bool dry_run=false;
  if (!dry_run) {
    if (!db2->changeFile(QString(filenameout.c_str()))) {
      cerr << "Error: File could not be saved: " << filenameout << "\n";
      exit(1);
    }
    if (!db2->save()) {
      cerr << "Error: Database could not be saved.\n";
      exit(1);
    }
  }

  return(0);
}
