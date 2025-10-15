#pragma once
#include <string>
#include <map>
#include "controllers.h"

struct RouteMatch { std::string path; HttpMethod method; };
RouteMatch resolve_route(); // derives from PATH_INFO or query param r
