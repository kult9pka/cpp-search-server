//#include "search_server.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
    }
    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });
    }
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus status_, int rating) { return status_ == status; });
    }


    template<typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
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

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }
    template<typename Predicate>
    vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (predicate(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
                });
        }
        return matched_documents;
    }
};

template <typename Key, typename Value>
ostream& operator<<(ostream& out, const pair<Key, Value>& container) {
    out << container.first << ": "s << container.second;
    return out;
}

template <typename Element>
void Print(ostream& out, const Element& container) {
    bool is_first = true;
    for (const auto& element : container) {
        if (is_first) {
            out << element;
            is_first = false;
        }
        else {
            out << ", "s << element;
        }
    }
}

template <typename Element>
ostream& operator<<(ostream& out, const vector<Element>& container) {
    out << "["s;
    Print(out, container);
    out << "]"s;
    return out;
}

template <typename Element>
ostream& operator<<(ostream& out, const set<Element>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

template <typename Key, typename Value>
ostream& operator<<(ostream& out, const map<Key, Value>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

//общие тесты
template <typename T>
void RunTestImpl(const T& func, const string& func_str) {
    cerr << func_str;
    func();
    cerr << " OK"s << endl;
}

#define RUN_TEST(func)  RunTestImpl((func), #func);

//сравнение значений
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file, const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, hint)

//истинность условия
void AssertImpl(bool value, const string& value_str, const string& file, const string& func, unsigned line, const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << value_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// -------- Начало модульных тестов поисковой системы ----------

//проверка добавления документов
void TestFindingDocumentInAddedDocument() {
    const int doc_id = 5;
    const string content = "green parrot from madagascar"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;

        ASSERT_EQUAL(server.FindTopDocuments("green"s).size(), 0);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.FindTopDocuments("green"s).size(), 1u);
        const auto doc = server.FindTopDocuments("green"s);
        const Document& doc0 = doc[0];
        ASSERT_EQUAL(doc0.id, doc_id);
        ASSERT_EQUAL(server.FindTopDocuments("parrot"s).size(), 1u);
        ASSERT_EQUAL(server.FindTopDocuments("from"s).size(), 1u);
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

void TestExcludingMinusWordsInAddedDocument() {
    const int doc_id = 0;
    const string content = "green parrot from madagascar"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("parrot");
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("-parrot");
        ASSERT(found_docs.empty());
    }
}

void TestMatchingDocumentsToSearchQuery() {
    const vector<int> ratings = { 1, 2, 3 };
    const vector<string> expected_result = { "green"s,"parrot"s };
    {
        SearchServer server;
        server.AddDocument(0, "green parrot from madagascar"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(1, "blue parrot from africa"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, "red parrot from indonesia"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(3, "grey hedgehod from russia"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(4, "white bear from north pole"s, DocumentStatus::ACTUAL, ratings);

        const auto [words, status] = server.MatchDocument("green parrot"s, 0);
        ASSERT_EQUAL(words, expected_result);
        ASSERT_EQUAL(words.size(), 2u);

        const auto [words1, status1] = server.MatchDocument("-blue parrot"s, 1);
        ASSERT(words1.empty());
    }
}

void TestSortingFoundDocsByRelevance() {
    SearchServer server;
    const vector<int> ratings = { 1, 2, 3 };
    {
        server.AddDocument(6, "green parrot from madagascar"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(9, "blue parrot from africa"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(4, "red parrot from indonesia"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, "grey hedgehod from russia"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(0, "white bear from north pole"s, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments("green parrot"s);
        const auto& doc0 = found_docs[0];
        const auto& doc1 = found_docs[1];
        const auto& doc2 = found_docs[2];

        ASSERT_EQUAL(found_docs.size(), 3u);

        ASSERT_EQUAL(doc0.id, 6);

        ASSERT_EQUAL(doc1.id, 4);

        ASSERT_EQUAL(doc2.id, 9);
    }
}

void TestCalculatingRatingInFoundDocs() {
    const int doc_id = 5;
    const string content = "green parrot from madagascar"s;
    const vector<int> ratings = { 1, 2, 3 };
    const vector<int> minus_ratings = { -1, -2, -3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto doc = server.FindTopDocuments("parrot"s);
        ASSERT_EQUAL(doc.size(), 1u);
        const Document& doc0 = doc[0];
        ASSERT_EQUAL(doc0.rating, (1 + 2 + 3) / 3);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, minus_ratings);
        const auto doc = server.FindTopDocuments("parrot"s);
        ASSERT_EQUAL(doc.size(), 1u);
        const Document& doc0 = doc[0];
        ASSERT_EQUAL(doc0.rating, (-1 + -2 + -3) / 3);
    }
}

void TestFilteringFoundDocsByPredicate() {
    const string content = "green parrot from madagascar"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;

        server.AddDocument(0, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(1, content, DocumentStatus::BANNED, ratings);
        server.AddDocument(2, content, DocumentStatus::IRRELEVANT, ratings);
        server.AddDocument(3, content, DocumentStatus::REMOVED, ratings);
        server.AddDocument(4, content, DocumentStatus::ACTUAL, ratings);

        ASSERT_EQUAL(server.FindTopDocuments("green"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; }).size(), 2u);

        ASSERT_EQUAL(server.FindTopDocuments("green"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::BANNED; }).size(), 1u);

        ASSERT_EQUAL(server.FindTopDocuments("green"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::IRRELEVANT; }).size(), 1u);

        ASSERT_EQUAL(server.FindTopDocuments("green"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::REMOVED; }).size(), 1u);
    }
}

void TestSearchingInFoundDocsByStatus() {
    const string content = "green parrot from madagascar"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;

        server.AddDocument(0, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(1, content, DocumentStatus::BANNED, ratings);
        server.AddDocument(2, content, DocumentStatus::IRRELEVANT, ratings);
        server.AddDocument(3, content, DocumentStatus::REMOVED, ratings);
        server.AddDocument(4, content, DocumentStatus::ACTUAL, ratings);

        ASSERT_EQUAL(server.FindTopDocuments("green"s, DocumentStatus::ACTUAL).size(), 2u);

        ASSERT_EQUAL(server.FindTopDocuments("green"s, DocumentStatus::BANNED).size(), 1u);

        ASSERT_EQUAL(server.FindTopDocuments("green"s, DocumentStatus::IRRELEVANT).size(), 1u);

        ASSERT_EQUAL(server.FindTopDocuments("green"s, DocumentStatus::REMOVED).size(), 1u);
    }
}

void TestCalculatingRelevanceInFoundDocs() {
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(0, "green parrot from madagascar"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(1, "blue parrot from africa"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, "red parrot from indonesia"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(3, "grey hedgehod from russia"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(4, "white bear from north pole"s, DocumentStatus::ACTUAL, ratings);

        const auto doc = server.FindTopDocuments("green parrot"s);

        const Document& doc0 = doc[0];
        double rel_0 = log(5. / 1.) * 1. / 4. + log(5. / 3.) * 1. / 4.;
        ASSERT((doc0.relevance - rel_0) < 1e-6);

        const Document& doc1 = doc[1];
        double rel_1 = log(5. / 3.) * 1. / 4.;
        ASSERT((doc1.relevance - rel_1) < 1e-6);

        const Document& doc2 = doc[2];
        double rel_2 = log(5. / 3.) * 1. / 4.;
        ASSERT((doc2.relevance - rel_2) < 1e-6);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestFindingDocumentInAddedDocument);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludingMinusWordsInAddedDocument);
    RUN_TEST(TestMatchingDocumentsToSearchQuery);
    RUN_TEST(TestSortingFoundDocsByRelevance);
    RUN_TEST(TestCalculatingRatingInFoundDocs);
    RUN_TEST(TestFilteringFoundDocsByPredicate);
    RUN_TEST(TestSearchingInFoundDocsByStatus);
    RUN_TEST(TestCalculatingRelevanceInFoundDocs);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}