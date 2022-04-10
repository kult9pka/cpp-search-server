#include "search_server.h"
#include "string_processing.h"
#include <fstream>

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor
                                                     // from string container
{
}

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status,
    const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.push_back(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}
int SearchServer::GetDocumentCount() const {
    return documents_.size();
}
int SearchServer::GetDocumentId(int index) const {
    return document_ids_.at(index);
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query,
    int document_id) const {
    const auto query = ParseQuery(raw_query);

    vector<string> matched_words;
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return { matched_words, documents_.at(document_id).status };
}



bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}
bool SearchServer::IsValidWord(const string& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}
vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + word + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}
int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const string& text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    string word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word "s + text + " is invalid");
    }
    return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    Query result;
    for (const string& word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            }
            else {
                result.plus_words.insert(query_word.data);
            }
        }
    }
    return result;
}
// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
/*
vector<int>::const_iterator SearchServer::begin() {
    return document_ids_.begin();
}

vector<int>::const_iterator SearchServer::end() {
    return document_ids_.end();
}
*/

vector<int>::iterator SearchServer::begin() {
    return document_ids_.begin();
}

vector<int>::iterator SearchServer::end() {
    return document_ids_.end();
}

map<string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static map<string, double> word_frequencies;
    word_frequencies.clear();
    for (const auto& doc : word_to_document_freqs_) {
        for (const auto& doc_ : doc.second) {
            if (document_id == doc_.first) {
                word_frequencies.emplace(doc.first, doc_.second);
            }
            else {
                continue;
            }
        }
    }
    return word_frequencies;
}

void SearchServer::RemoveDocument(int document_id) {
    if(documents_.count(document_id))
        documents_.erase(document_id);    

    if(document_to_word_freqs_.count(document_id))
        document_to_word_freqs_.erase(document_id);    

    if(count(document_ids_.begin(), document_ids_.end(), document_id))
        document_ids_.erase(find_if(document_ids_.begin(), document_ids_.end(), [&document_id](int i) {
        return i == document_id;
            }));    
    
    for (auto& doc : word_to_document_freqs_) {
        auto it = doc.second.find(document_id);
        if (it != doc.second.end())
            doc.second.erase(it);
    }
    auto it = find_if(word_to_document_freqs_.begin(), word_to_document_freqs_.end(), [](auto& doc) {
        return doc.second.empty();
        });
    if(it != word_to_document_freqs_.end())
        word_to_document_freqs_.erase(it);    
}

/*
void SearchServer::PrintBefore() {
    ofstream file1("Before.txt");
    file1 << "map<int, DocumentData> documents_:"s << endl;
    for (const auto& doc : documents_) {
        file1 << doc.first << " "s;
    }
    file1 << endl << endl;

    file1 << "vector<int> document_ids_"s << endl;
    for (const auto& doc : document_ids_) {
        file1 << doc << " "s;
    }
    file1 << endl << endl;

    file1 << "map<int, map<string, double>> document_to_word_freqs_:"s << endl;
    for (const auto& doc : document_to_word_freqs_) {
        file1 << "id:"s << doc.first << endl;
        for (const auto& doc_ : doc.second) {
            file1 << doc_.first << " "s << doc_.second << endl;
        }
        file1 << endl;
    }

    file1 << "map<string, map<int, double>> word_to_document_freqs_:"s << endl;
    for (const auto& some_doc : word_to_document_freqs_) {
        file1 << "word: "s << some_doc.first << "\n"s;
        for (const auto& into_some : some_doc.second) {
            file1 << "id "s << into_some.first << " freq "s << into_some.second << "\n";
        }
        file1 << endl;
    }

}
void SearchServer::PrintAfter() {
    ofstream file2("After.txt");
    file2 << "map<int, DocumentData> documents_:"s << endl;
    for (const auto& doc : documents_) {
        file2 << doc.first << " "s;
    }
    file2 << endl << endl;

    file2 << "vector<int> document_ids_"s << endl;
    for (const auto& doc : document_ids_) {
        file2 << doc << " "s;
    }
    file2 << endl << endl;

    file2 << "map<int, map<string, double>> document_to_word_freqs_:"s << endl;
    for (const auto& doc : document_to_word_freqs_) {
        file2 << "id:"s << doc.first << endl;
        for (const auto& doc_ : doc.second) {
            file2 << doc_.first << " "s << doc_.second << endl;
        }
        file2 << endl;
    }

    file2 << "map<string, map<int, double>> word_to_document_freqs_:"s << endl;
    for (const auto& some_doc : word_to_document_freqs_) {
        file2 << "word: "s << some_doc.first << "\n"s;
        for (const auto& into_some : some_doc.second) {
            file2 << "id "s << into_some.first << " freq "s << into_some.second << "\n";
        }
        file2 << endl;
    }
}
*/