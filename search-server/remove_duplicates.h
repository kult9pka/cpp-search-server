#pragma once

#include <map>
#include <string>
#include <set>
#include <iostream>

#include "search_server.h"

bool operator==(const std::map<std::string, double> lhs, const std::map<std::string, double> rhs);

void RemoveDuplicates(SearchServer& search_server);