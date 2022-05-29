#include "search_server.h"
#include "string_processing.h"
#include <fstream>

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor
                                                     // from string container
{
}

SearchServer::SearchServer(const std::string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
{
}

void SearchServer::AddDocument(int document_id, const string_view document, DocumentStatus status,
    const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string_view word : words) {
        storage.emplace(storage.begin(), word);   //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!   EDITED
        word_to_document_freqs_[storage.front()][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][storage.front()] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}
int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view raw_query,
    int document_id) const {

    if (document_ids_.count(document_id) == 0) {
        throw out_of_range("out_of_range");
    }

    const auto query = ParseQuery(raw_query);

    for (const string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return { {}, documents_.at(document_id).status };
        }
    }

    vector<string_view> matched_words;
    for (const string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    return { matched_words, documents_.at(document_id).status };
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(execution::sequenced_policy,
    const string_view raw_query,
    int document_id) const {

    if (document_ids_.count(document_id) == 0) {
        throw out_of_range("out_of_range");
    }

    const auto query = ParseQueryPar(execution::seq, raw_query);
    for (const string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return { {}, documents_.at(document_id).status };
        }
    }

    vector<string_view> matched_words;
    for (const string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    return { matched_words, documents_.at(document_id).status };
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy,
    const string_view raw_query,
    int document_id) const {

    if (document_ids_.count(document_id) == 0) {
        throw out_of_range("out_of_range");
    }

    const auto query = ParseQueryPar(execution::par, raw_query);

    bool statement = any_of(
        execution::par,
        query.minus_words.begin(), query.minus_words.end(),
        [&freqs = word_to_document_freqs_, document_id](const string_view word) {
            return (freqs.count(word) && freqs.at(word).count(document_id));
        }
    );

    if (statement) {
        return { {}, documents_.at(document_id).status };
    }

    vector<string_view> matched_words(query.plus_words.size());
    auto matched_end = copy_if(
        execution::par,
        query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(),
        [&freqs = word_to_document_freqs_, document_id](const string_view word) {
            return (freqs.count(word) && freqs.at(word).count(document_id));
        }
    );
    //bool state = count_if(
    //    execution::par,
    //    matched_end, matched_words.end(),
    //    [](const string_view word) {
    //        return word == ""s;
    //    }
    //);
    //if (state) {
    //    matched_words.erase(matched_end, matched_words.end());
    //}

    //sort(execution::par, matched_words.begin(), matched_words.end());
    //auto unique_words_end = unique(execution::par, matched_words.begin(), matched_words.end());
    //matched_words.erase(unique_words_end, matched_words.end());

    sort(execution::par, matched_words.begin(), matched_end);
    auto unique_words_end = unique(execution::par, matched_words.begin(), matched_end);
    matched_words.erase(unique_words_end, matched_words.end());

    return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(const string_view word) const {
    return stop_words_.count(string{ word }) > 0;
}
bool SearchServer::IsValidWord(const string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}
vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (const string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + string{ word } + " is invalid"s);
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

SearchServer::QueryWord SearchServer::ParseQueryWord(const string_view text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word "s + string{ text } + " is invalid");
    }
    return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
    Query result;
    for (const string_view word : SplitIntoWords(text)) {
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

SearchServer::ParQuery SearchServer::ParseQueryPar(execution::sequenced_policy, string_view text) const {
    ParQuery result;
    vector<string_view> words = SplitIntoWords(text);

    sort(words.begin(), words.end());
    auto iter_words = unique(words.begin(), words.end());
    words.erase(iter_words, words.end());

    for (const string_view word : words) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    return result;
}

SearchServer::ParQuery SearchServer::ParseQueryPar(execution::parallel_policy, string_view text) const {
    ParQuery result;

    for (const string_view word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    return result;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

set<int>::iterator SearchServer::begin() {
    return document_ids_.begin();
}

set<int>::iterator SearchServer::end() {
    return document_ids_.end();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string_view, double> word_frequencies;
    if (document_to_word_freqs_.count(document_id)) {
        return document_to_word_freqs_.at(document_id);
    }
    return word_frequencies;
}

void SearchServer::RemoveDocument(int document_id) {
    if (document_to_word_freqs_.count(document_id)) {
        for (const auto& [target_word, _] : document_to_word_freqs_.at(document_id)) {
            auto& doc_by_target_word = word_to_document_freqs_.at(target_word);
            doc_by_target_word.erase(document_id);
            if (doc_by_target_word.empty())
                word_to_document_freqs_.erase(target_word);
        }
    }

    documents_.erase(document_id);
    document_to_word_freqs_.erase(document_id);
    document_ids_.erase(document_id);
}