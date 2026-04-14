#pragma once
#include <string>

// Package management commands
int pkgGet(const std::string& spec);      // to get user/repo[@version]
int pkgRemove(const std::string& name);    // to remove name
int pkgList();                             // to list
int pkgInit();                             // to init (create to.pkg)
