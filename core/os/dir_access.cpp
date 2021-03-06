/*************************************************************************/
/*  dir_access.cpp                                                       */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                    http://www.godotengine.org                         */
/*************************************************************************/
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                 */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
#include "dir_access.h"
#include "os/file_access.h"
#include "os/memory.h"
#include "os/os.h"
#include "globals.h"


String DirAccess::_get_root_path() const {

	switch(_access_type) {

		case ACCESS_RESOURCES: return Globals::get_singleton()->get_resource_path();
		case ACCESS_USERDATA: return OS::get_singleton()->get_data_dir();
		default: return "";
	}

	return "";
}
String DirAccess::_get_root_string() const {

	switch(_access_type) {

		case ACCESS_RESOURCES: return "res://";
		case ACCESS_USERDATA: return "user://";
		default: return "";
	}

	return "";
}


static Error _erase_recursive(DirAccess *da) {

	List<String> dirs;
	List<String> files;

	da->list_dir_begin();
	String n = da->get_next();
	while(n!=String()) {

		if (n!="." && n!="..") {

			if (da->current_is_dir())
				dirs.push_back(n);
			else
				files.push_back(n);
		}

		n=da->get_next();
	}

	da->list_dir_end();

	for(List<String>::Element *E=dirs.front();E;E=E->next()) {

		Error err = da->change_dir(E->get());
		if (err==OK) {

			err = _erase_recursive(da);
			if (err) {
				print_line("err recurso "+E->get());
				return err;
			}
			err = da->change_dir("..");
			if (err) {
				print_line("no go back "+E->get());
				return err;
			}
			err = da->remove(da->get_current_dir().plus_file(E->get()));
			if (err) {
				print_line("no remove dir"+E->get());
				return err;
			}
		} else {
			print_line("no change to "+E->get());
			return err;
		}
	}

	for(List<String>::Element *E=files.front();E;E=E->next()) {

		Error err = da->remove(da->get_current_dir().plus_file(E->get()));
		if (err) {

			print_line("no remove file"+E->get());
			return err;
		}
	}

	return OK;
}


Error DirAccess::erase_contents_recursive() {

	return _erase_recursive(this);
}


Error DirAccess::make_dir_recursive(String p_dir) {

	if (p_dir.length() < 1) {
		return OK;
	};

	String full_dir;
	Globals* g = Globals::get_singleton();

	if (!p_dir.is_abs_path()) {
		//append current

		String cur = normalize_path(g->globalize_path(get_current_dir()));
		if (cur[cur.length()-1] != '/') {
			cur = cur + "/";
		};

		full_dir=(cur+"/"+p_dir).simplify_path();
	} else {
		//validate and use given
		String dir = normalize_path(g->globalize_path(p_dir));
		if (dir.length() < 1) {
			return OK;
		};
		if (dir[dir.length()-1] != '/') {
			dir = dir + "/";
		};
		full_dir=dir;
	}

	//int slices = full_dir.get_slice_count("/");

	int pos = 0;
	while (pos < full_dir.length()) {

		int n = full_dir.find("/", pos);
		if (n < 0) {
			n = full_dir.length();
		};
		pos = n + 1;

		if (pos > 1) {
			String to_create = full_dir.substr(0, pos -1);
			//print_line("MKDIR: "+to_create);
			Error err = make_dir(to_create);
			if (err != OK && err != ERR_ALREADY_EXISTS) {

				ERR_FAIL_V(err);
			};
		};
	};

	return OK;
};


String DirAccess::normalize_path(const String &p_path) {

	static const int max_depth = 64;
	int pos_stack[max_depth];
	int curr = 0;

	int pos = 0;
	String cur_dir;

	do {

		if (curr >= max_depth) {

			ERR_PRINT("Directory depth too deep.");
			return "";
		};

		int start = pos;

		int next = p_path.find("/", pos);
		if (next < 0) {
			next = p_path.length() - 1;
		};

		pos = next + 1;

		cur_dir = p_path.substr(start, next - start);

		if (cur_dir == "" || cur_dir == ".") {
			continue;
		};
		if (cur_dir == "..") {

			if (curr > 0) { // pop a dir
				curr -= 2;
			};
			continue;
		};

		pos_stack[curr++] = start;
		pos_stack[curr++] = next;

	} while (pos < p_path.length());

	String path;
	if (p_path[0] == '/') {
		path = "/";
	};

	int i=0;
	while (i < curr) {

		int start = pos_stack[i++];

		while ( ((i+1)<curr) && (pos_stack[i] == pos_stack[i+1]) ) {

			++i;
		};
		path = path + p_path.substr(start, (pos_stack[i++] - start) + 1);
	};

	return path;
};

String DirAccess::get_next(bool* p_is_dir) {

	String next=get_next();
	if (p_is_dir)
		*p_is_dir=current_is_dir();
	return next;
}

String DirAccess::fix_path(String p_path) const {

	switch(_access_type) {

		case ACCESS_RESOURCES: {

			if (Globals::get_singleton()) {
				if (p_path.begins_with("res://")) {

					String resource_path = Globals::get_singleton()->get_resource_path();
					if (resource_path != "") {

						return p_path.replace("res:/",resource_path);
					};
					return p_path.replace("res://", "");
				}
			}


		} break;
		case ACCESS_USERDATA: {


			if (p_path.begins_with("user://")) {

				String data_dir=OS::get_singleton()->get_data_dir();
				if (data_dir != "") {

					return p_path.replace("user:/",data_dir);
				};
				return p_path.replace("user://", "");
			}

		} break;
		case ACCESS_FILESYSTEM: {

			return p_path;
		} break;
	}

	return p_path;
}


DirAccess::CreateFunc DirAccess::create_func[ACCESS_MAX]={0,0,0};

DirAccess *DirAccess::create_for_path(const String& p_path) {

	DirAccess *da=NULL;
	if (p_path.begins_with("res://")) {

		da=create(ACCESS_RESOURCES);
	} else if (p_path.begins_with("user://")) {

		da=create(ACCESS_USERDATA);
	} else {

		da=create(ACCESS_FILESYSTEM);
	}

	return da;
}

DirAccess *DirAccess::open(const String& p_path,Error *r_error) {

	DirAccess *da=create_for_path(p_path);

	ERR_FAIL_COND_V(!da,NULL);
	Error err = da->change_dir(p_path);
	if (r_error)
		*r_error=err;
	if (err!=OK) {
		memdelete(da);
		return NULL;
	}

	return da;
}

DirAccess *DirAccess::create(AccessType p_access) {
	
	DirAccess * da = create_func[p_access]?create_func[p_access]():NULL;
	if (da) {
		da->_access_type=p_access;
	}

	return da;
};


String DirAccess::get_full_path(const String& p_path,AccessType p_access) {

	DirAccess *d=DirAccess::create(p_access);
	if (!d)
		return p_path;
		
	d->change_dir(p_path);
	String full=d->get_current_dir();
	memdelete(d);
	return full;
}

Error DirAccess::copy(String p_from,String p_to) {
	
	//printf("copy %s -> %s\n",p_from.ascii().get_data(),p_to.ascii().get_data());
	Error err;
	FileAccess *fsrc = FileAccess::open(p_from, FileAccess::READ,&err);
	
	if (err) {

		ERR_FAIL_COND_V( err, err );
	}

	
	FileAccess *fdst = FileAccess::open(p_to, FileAccess::WRITE,&err );
	if (err) {
						
		fsrc->close();
		memdelete( fsrc );
		ERR_FAIL_COND_V( err, err );
	}
	
	fsrc->seek_end(0);
	int size = fsrc->get_pos();
	fsrc->seek(0);
	err = OK;
	while(size--) {
		
		if (fsrc->get_error()!=OK) {
			err= fsrc->get_error();
			break;
		}
		if (fdst->get_error()!=OK) {
			err= fdst->get_error();
			break;
		}
		
		fdst->store_8( fsrc->get_8() );
	}
	
	memdelete(fsrc);
	memdelete(fdst);
	
	return err;
}

bool DirAccess::exists(String p_dir) {

	DirAccess* da = DirAccess::create_for_path(p_dir);
	bool valid = da->change_dir(p_dir)==OK;
	memdelete(da);
	return valid;

}

DirAccess::DirAccess(){

	_access_type=ACCESS_FILESYSTEM;
}


DirAccess::~DirAccess() {

}


