#include "remove_duplicates.h"
#include "log_duration.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
	set<int> duplicate;
	map<set<string>, int> unique;
	for (const auto& s : search_server) {
		set<string> some;
		auto& v = search_server.GetWordFrequencies(s);
		for (auto it = v.begin(); it != v.end(); ++it) {
			some.insert(it->first);
		}
		if (unique.empty()) {
			unique.insert({ some, s });
		}
		else {
			if (unique.count(some)) {
				duplicate.insert(s);
			}
			else {
				unique.insert({ some, s });
			}
		}
	}
	if (!duplicate.empty()) {
		for (const auto& num : duplicate) {
			cout << "Found duplicate document id "s << num << endl;
			search_server.RemoveDocument(num);
		}
	}
}

