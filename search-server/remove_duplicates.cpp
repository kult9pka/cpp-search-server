#include "remove_duplicates.h"

using namespace std;

bool operator==(const map<string, double> lhs, const map<string, double> rhs) {
	if (!(lhs.size() == rhs.size()))
		return false;
	set<string> tmp1, tmp2;
	for (const auto& doc : lhs) {
		tmp1.insert(doc.first);
	}
	for (const auto& doc : rhs) {
		tmp2.insert(doc.first);
	}
	if (tmp1 != tmp2)
		return false;
	return true;
}

void RemoveDuplicates(SearchServer& search_server) {
	set<int> duplicate;
	for (auto iter1 = search_server.begin(); iter1 != search_server.end(); ++iter1)
		for (auto iter2 = iter1 + 1; iter2 != search_server.end(); ++iter2)
			if (search_server.GetWordFrequencies(*iter1) == search_server.GetWordFrequencies(*iter2))
				duplicate.insert(*iter2);
	if (!duplicate.empty()) {
		for (const auto& num : duplicate) {
			cout << "Found duplicate document id "s << num << endl;
			search_server.RemoveDocument(num);
		}
	}
}