#include "remove_duplicates.h"
#include "log_duration.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
	//прошу прощения, решение как всегда пришло в голову именно тогда, когда уже начал сильно опаздывать на работу :(
	//поэтому я гнался за работоспособностью кода, а не его эстетическим видом
	//отсюда и появились всякие s, v, some и прочие непонятные символы :)  
	
	set<int> duplicate_documents_ids;
	map<set<string>, int> unique_documents;
	for (const auto& id : search_server) {
		set<string> set_of_words;
		const auto& doc_to_check = search_server.GetWordFrequencies(id);

		//не понимаю, как применить std::transform, поэтому range-based for:
		for (const auto& [words, _] : doc_to_check) {
			set_of_words.insert(words);
		}				
		if (unique_documents.empty()) {
			unique_documents.insert({ set_of_words, id });
		}
		else {
			if (unique_documents.count(set_of_words)) {
				duplicate_documents_ids.insert(id);
			}
			else {
				unique_documents.insert({ set_of_words, id });
			}
		}
	}
	if (!duplicate_documents_ids.empty()) {
		for (const auto& id : duplicate_documents_ids) {
			cout << "Found duplicate document id "s << id << endl;
			search_server.RemoveDocument(id);
		}
	}
}

