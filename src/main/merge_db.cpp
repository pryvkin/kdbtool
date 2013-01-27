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

#include <unordered_map>
#include <vector>
#include <iostream>
using namespace std;

#include "merge_db.h"

typedef unordered_map< string, IGroupHandle* > GroupMap;
struct GroupAction {
  enum GroupActionType { ADD };
  GroupActionType action;
  IGroupHandle *src;
  IGroupHandle *dest_grp;    // ignored if dest is root
  IDatabase *dest_db;        // set to !NULL if dest is root
  GroupAction(GroupActionType a, IGroupHandle *s, IGroupHandle *dg) :
    action(a), src(s), dest_grp(dg), dest_db(NULL) { }
  GroupAction(GroupActionType a, IGroupHandle *s, IDatabase *dd) :
    action(a), src(s), dest_grp(NULL), dest_db(dd) { }

};
typedef vector<GroupAction> GroupActionList;

struct EntryAction {
  enum EntryActionType { ADD, MERGE };
  EntryActionType action;
  IEntryHandle *src;
  IGroupHandle *dest_grp;
  IEntryHandle *dest_entry;
  EntryAction(EntryActionType a, IEntryHandle *s, IGroupHandle *dg) :
    action(a), src(s), dest_grp(dg), dest_entry(NULL) { }
  EntryAction(EntryActionType a, IEntryHandle *s, IEntryHandle *de) :
    action(a), src(s), dest_grp(NULL), dest_entry(de) { }
};
typedef vector<EntryAction> EntryActionList;

////////////

GroupMap get_subgroup_names(const QList<IGroupHandle*>& children,
			    bool root_only=false) {
  GroupMap result;
  for(int i=0; i < children.size(); ++i) {
    if (root_only && children[i]->parent() != NULL)
      continue;
    string key(children[i]->title().toUtf8().data());
    if (key != "Backup")
      result[key] = children[i];
  }
  return(result);
}
GroupMap get_subgroup_names(IGroupHandle* parent) {
  return( get_subgroup_names(parent->children()) );
}
GroupMap get_subgroup_names(IDatabase* root) {
  return( get_subgroup_names(root->groups(), true) );
}

// go through subgroups of src_parent_grps and if they are missing
// from dest_parent_grp, add them
// if either is NULL, toplevel groups are used
void add_missing_groups(IDatabase* src_db, IDatabase *dest_db,
			IGroupHandle* src_parent_grp,
			IGroupHandle* dest_parent_grp,
			GroupActionList &actions) {
  bool is_toplevel( src_parent_grp == NULL);
  GroupMap src_groups, dest_groups;
  if (is_toplevel) {
    src_groups = get_subgroup_names(src_db);
    dest_groups = get_subgroup_names(dest_db);
  } else {
    src_groups = get_subgroup_names(src_parent_grp);
    dest_groups = get_subgroup_names(dest_parent_grp);
  }

  for( GroupMap::iterator src_grp = src_groups.begin(); 
       src_grp != src_groups.end(); ++src_grp ) {
    GroupMap::iterator src_in_dest( dest_groups.find(src_grp->first) );
    if (src_in_dest != dest_groups.end() ) {
      //cerr << "Found " << src_grp->first  << "in dest" 
      //	   << dest->title().toUtf8().data();
      // group in both src and dest
      // check the subgroups
      add_missing_groups(src_db, dest_db, src_grp->second,
			 src_in_dest->second, actions);
    } else {
      // add the src grp to the dest groups
      if (is_toplevel) {
	actions.push_back(GroupAction(GroupAction::ADD, src_grp->second,
				      dest_db));
      } else {
	actions.push_back(GroupAction(GroupAction::ADD, src_grp->second,
				      dest_parent_grp));
      }
    }
  }
}

///// entry merging

typedef unordered_map< string, IEntryHandle* > EntryMap;

string get_entry_key(IEntryHandle *e) {
  string key(e->title().toUtf8().data());
  key += "$$$";
  key += e->username().toUtf8().data();
  return(key);
}

EntryMap get_group_entries(IDatabase *db, IGroupHandle* grp) {
  // take the most recent entry
  QList<IEntryHandle*> entries( db->entries(grp) );
  EntryMap result;
  for(int i=0; i < entries.size(); ++i) {
    string key( get_entry_key( entries[i] ) );
    result[key] = entries[i];
  }
  return(result);
}

void merge_entry(IEntryHandle* src_entry, IEntryHandle* dest_entry,
		 EntryActionList& actions) {
  // merge 2 entries by taking the most recently modified one
  if (src_entry->lastMod() > dest_entry->lastMod() )
    actions.push_back(EntryAction(EntryAction::MERGE, src_entry, dest_entry));
}

// UGLY CODE DUPLICATION AGAIN DUE TO ROOT SPECIAL CASE
/*void merge_entries(IDatabase *src_db, IDatabase *dest_db,
		   IGroupHandle *src_supergrp, IGroupHandle *dest_supergrp,
		   EntryActionList &entry_actions) {
  GroupMap src_groups( get_subgroup_names(src_supergrp) );
    for( GroupMap::iterator src_grp_it = src_groups.begin(); 
       src_grp_it != src_groups.end(); ++src_grp_it ) {
      string src_grp_name( src_grp_it->first );
      IGroupHandle *src_grp = src_grp_it->second;

      GroupMap dest_groups( get_subgroup_names(dest_supergrp) );
      IGroupHandle *dest_grp = dest_groups[src_grp_name];

      EntryMap src_entries = get_group_entries(src_db, src_grp);
      EntryMap dest_entries = get_group_entries(dest_db, dest_grp);

      for (EntryMap::iterator src_entry_it = src_entries.begin();
	   src_entry_it != src_entries.end(); ++src_entry_it) {
	string src_key( src_entry_it->first );
	IEntryHandle *src_entry (src_entry_it->second);

	EntryMap::iterator src_in_dest = dest_entries.find(src_key);
	if (src_in_dest == dest_entries.end()) {
	  // entry missing from dest
	  entry_actions.push_back(EntryAction(EntryAction::ADD,
					      src_entry, dest_grp));
	} else {
	  IEntryHandle *dest_entry(src_in_dest->second);
	  // entry exists in both
	  merge_entry(src_entry, dest_entry, src_key, entry_actions);
	}
      }  // for src entries in src group

      merge_entries(src_db, dest_db, src_grp, dest_grp, entry_actions);

    }  // for src subgroups

}*/

// set src_parent_grp = NULL if these are toplevel groups
void merge_entries(IDatabase *src_db, IDatabase *dest_db, 
		   IGroupHandle *src_parent_grp, IGroupHandle *dest_parent_grp,
		   EntryActionList& actions) {
  bool is_toplevel( src_parent_grp == NULL);
  GroupMap src_groups;
  if (is_toplevel)
    src_groups = get_subgroup_names(src_db);
  else
    src_groups = get_subgroup_names(src_parent_grp);

    for( GroupMap::iterator src_grp_it = src_groups.begin(); 
       src_grp_it != src_groups.end(); ++src_grp_it ) {
      string src_grp_name( src_grp_it->first );
      IGroupHandle *src_grp = src_grp_it->second;

      GroupMap dest_groups;
      if (is_toplevel)
	dest_groups = get_subgroup_names(dest_db);
      else
	dest_groups = get_subgroup_names(dest_parent_grp);
      IGroupHandle *dest_grp = dest_groups[src_grp_name];

      EntryMap src_entries = get_group_entries(src_db, src_grp);
      EntryMap dest_entries = get_group_entries(dest_db, dest_grp);

      for (EntryMap::iterator src_entry_it = src_entries.begin();
	   src_entry_it != src_entries.end(); ++src_entry_it) {
	string src_key( src_entry_it->first );
	IEntryHandle *src_entry (src_entry_it->second);

	EntryMap::iterator src_in_dest = dest_entries.find(src_key);
	if (src_in_dest == dest_entries.end()) {
	  // entry missing from dest
	  actions.push_back(EntryAction(EntryAction::ADD,
					      src_entry, dest_grp));
	} else {
	  // entry exists in both
	  merge_entry(src_entry, src_in_dest->second, actions);
	}
      }  // for src entries in src group

      merge_entries(src_db, dest_db, src_grp, dest_grp, actions);

    }  // for src subgroups
}


///// main merge

void merge_dbs(IDatabase *src, IDatabase *dest) {
  GroupActionList group_actions;
 
  add_missing_groups(src, dest, NULL, NULL, group_actions);

  for(int i=0; i < group_actions.size(); ++i) {
    GroupAction &ga(group_actions[i]);
    cerr << "Adding group \"" << ga.src->title().toUtf8().data()
	 << "\" -> \""
	 << (ga.dest_db ? "(ROOT)" : ga.dest_grp->title().toUtf8().data())
	 << "\"\n";
    CGroup new_grp;
    new_grp.Id = 0;  // should be unused
    new_grp.Image = ga.src->image();
    new_grp.Title = ga.src->title();
    dest->addGroup(&new_grp, ga.dest_grp);
  }

  EntryActionList entry_actions;
  merge_entries(src, dest, NULL, NULL, entry_actions);

  for(int i=0; i < entry_actions.size(); ++i) {
    EntryAction &ea(entry_actions[i]);
    if (ea.action == EntryAction::ADD) {
      cerr << "Adding entry \"" << ea.src->title().toUtf8().data()
	   << " / " << ea.src->username().toUtf8().data() << "\"\n";

      // this may not work for ptr-type members like Binary
      // since it's a default copy
      CEntry new_entry = ea.src->data();
      dest->addEntry(&new_entry, ea.dest_grp);

    } else if (ea.action == EntryAction::MERGE) {
      cerr << "Merging entry \"" << ea.src->title().toUtf8().data()
	   << " / " << ea.src->username().toUtf8().data() << "\"\n";
      IGroupHandle *dest_grp = ea.dest_entry->group();
      // this may not work for ptr-type members like Binary
      // since it's a default copy
      CEntry new_entry = ea.src->data();
      dest->addEntry(&new_entry, ea.dest_entry->group());
      dest->deleteEntry(ea.dest_entry);
    }
  }
}

