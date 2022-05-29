#pragma once
#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <utility>
#include <cmath>
#include <stdexcept>
#include <numeric>
#include <iterator>
#include <execution>
#include <list>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double ACCURACY = 1e-6;

class SearchServer {

public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(const std::string_view stop_words_text);

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;

    template <typename Policy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(Policy& policy, const std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename Policy>
    std::vector<Document> FindTopDocuments(Policy& policy, const std::string_view raw_query, DocumentStatus status) const;

    template <typename Policy>
    std::vector<Document> FindTopDocuments(Policy& policy, const std::string_view raw_query) const;

    int GetDocumentCount() const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy, const std::string_view raw_query, int document_id) const;

    std::set<int>::iterator begin();
    std::set<int>::iterator end();

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
    void RemoveDocument(int document_id);

    template<typename Policy>
    void RemoveDocument(Policy policy_, int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    const std::set<std::string> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    std::list<std::string> storage; //основное хранилище для строк!

    bool IsStopWord(const std::string_view word) const;
    static bool IsValidWord(const std::string_view word);
    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const std::string_view text) const;

    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };

    struct ParQuery {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view text) const;
    ParQuery ParseQueryPar(std::execution::sequenced_policy, std::string_view text) const;
    ParQuery ParseQueryPar(std::execution::parallel_policy, std::string_view text) const;

    template <typename Policy>
    ParQuery ParseQueryTop(Policy& policy, std::string_view text) const;

    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;

    template <typename Policy, typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(Policy& policy, const ParQuery& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        using namespace std::string_literals;
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query,
    DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            return lhs.relevance > rhs.relevance
                || (std::abs(lhs.relevance - rhs.relevance) < ACCURACY && lhs.rating > rhs.rating);
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;    
}

template<typename Policy>
void SearchServer::RemoveDocument(Policy policy_, int document_id) {
    if (document_to_word_freqs_.count(document_id)) {

        std::vector<const std::string*> to_delete(document_to_word_freqs_.at(document_id).size());
        std::transform(
            policy_,
            document_to_word_freqs_.at(document_id).begin(), document_to_word_freqs_.at(document_id).end(),
            to_delete.begin(),
            [](const std::pair<const std::string, double>& doc) {
                return &doc.first;
            }
        );
        for_each(
            policy_,
            to_delete.begin(), to_delete.end(),
            [&freqs = word_to_document_freqs_, document_id](const std::string* doc) {
                freqs.at(*doc).erase(document_id);
            }
        );
    }

    documents_.erase(document_id);
    document_to_word_freqs_.erase(document_id);
    document_ids_.erase(document_id);
}

template <typename Policy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(Policy& policy, const std::string_view raw_query, DocumentPredicate document_predicate) const {
    const auto query = ParseQueryTop(policy, raw_query);

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(policy, matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            return lhs.relevance > rhs.relevance
                || (std::abs(lhs.relevance - rhs.relevance) < ACCURACY && lhs.rating > rhs.rating);
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename Policy>
std::vector<Document> SearchServer::FindTopDocuments(Policy& policy, const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        policy,
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

template <typename Policy>
std::vector<Document> SearchServer::FindTopDocuments(Policy& policy, const std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename Policy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(Policy& policy, const ParQuery& query, DocumentPredicate document_predicate) const {

    ConcurrentMap<int, double> document_to_relevance(10); 

        std::for_each(
            policy,
            query.plus_words.begin(), query.plus_words.end(),
            [this, &document_to_relevance, &document_predicate, &policy](const std::string_view word) {
                if (word_to_document_freqs_.count(word)) {
                    const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                    std::for_each(
                        policy,
                        word_to_document_freqs_.at(word).begin(), word_to_document_freqs_.at(word).end(),
                        [this, &document_to_relevance, &document_predicate, &inverse_document_freq](const auto& pair_) {
                            const auto& document_data = documents_.at(pair_.first);
                            if (document_predicate(pair_.first, document_data.status, document_data.rating)) {
                                document_to_relevance[pair_.first].ref_to_value += pair_.second * inverse_document_freq;
                            }
                        }
                    );
                }
            }
        );  

    std::for_each(
        policy,
        query.minus_words.begin(), query.minus_words.end(),
        [this, &document_to_relevance, &policy](const std::string_view word) {
            if (word_to_document_freqs_.count(word)) {
                std::for_each(
                    policy,
                    word_to_document_freqs_.at(word).begin(), word_to_document_freqs_.at(word).end(),
                    [&document_to_relevance](const auto& pair_) {
                        document_to_relevance.erase(pair_.first);
                    }
                );
            }
        }
    );

    auto result = document_to_relevance.BuildOrdinaryMap();
    std::vector<Document> matched_documents(result.size());

    std::atomic_int num = 0;
    std::for_each(
        policy,
        result.begin(), result.end(),
        [this, &matched_documents, &num](const auto& pair_) {
            matched_documents[num++] = Document{ pair_.first, pair_.second, documents_.at(pair_.first).rating };
        }
    );

    return matched_documents;
}

template<typename Policy>
SearchServer::ParQuery SearchServer::ParseQueryTop(Policy& policy, std::string_view text) const {
    ParQuery result;
    std::vector<std::string_view> words = SplitIntoWords(text);

    std::sort(policy, words.begin(), words.end());
    auto iter_words = unique(words.begin(), words.end());
    words.erase(iter_words, words.end());

    for (const std::string_view word : words) {
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