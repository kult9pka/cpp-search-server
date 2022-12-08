#include "test_example_functions.h"

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
    const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    // ������� ����������, ��� ����� �����, �� ��������� � ������ ����-����,
    // ������� ������ ��������
    {
        SearchServer server(" "s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }
    // ����� ����������, ��� ����� ����� �� �����, ��������� � ������ ����-����,
    // ���������� ������ ���������
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
    {
        SearchServer server("-  -- "s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(!server.FindTopDocuments("in"s).empty());
    }
    {
        SearchServer server("in"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
    {
        SearchServer server("  "s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("       "s).empty());
    }
}

void TestMinusWords() {
    const int doc_id = 15;
    const vector<int> ratings = { 1, 2, 3 };
    {
        const string content = "cat in the city"s;
        SearchServer server(content);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("-in"s).empty());
    }
    {
        const string content = "cat in the city"s;
        SearchServer server(content);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("-cat -in -the -city"s).empty());
    }
}

void TestMatchedDocuments() {
    const int doc_id = 0;
    const string content = "b a ccc ddd"s;
    const vector<int> ratings = { 1, 2, 3 };

    {
        tuple<vector<string_view>, DocumentStatus> founding = { {"a", "b", "ccc", "ddd"}, DocumentStatus::ACTUAL };
        SearchServer server = SearchServer(" "s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        tuple<vector<string_view>, DocumentStatus> matched = server.MatchDocument("a b ccc ddd", doc_id);
        auto [matched_vector, matched_Document_Status] = matched;
        auto [founding_vector, founding_Document_Status] = founding;
        ASSERT_EQUAL(matched_vector, founding_vector);
    }

    {
        tuple<vector<string_view>, DocumentStatus> founding = { {"a", "b", "ccc"}, DocumentStatus::BANNED };
        SearchServer server = SearchServer(" "s);
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        tuple<vector<string_view>, DocumentStatus> matched = server.MatchDocument("a b ccc -ddd", doc_id);
        auto [matched_vector, matched_Document_Status] = matched;
        auto [founding_vector, founding_Document_Status] = founding;
        ASSERT(matched_vector != founding_vector);
    }
    // ASSERT_NON_EQUAL ��� ������������� ��������
    {
        tuple<vector<string_view>, DocumentStatus> founding = { {"b"}, DocumentStatus::ACTUAL };
        SearchServer server = SearchServer(" "s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        tuple<vector<string_view>, DocumentStatus> matched = server.MatchDocument("g b i m", doc_id);
        auto [matched_vector, matched_Document_Status] = matched;
        auto [founding_vector, founding_Document_Status] = founding;
        ASSERT_EQUAL(matched_vector, founding_vector);
    }
}

void TestSort() {
    const string Hint = "��������� ����������� �� ���������";
    {
        const string content = "� ��"s;
        SearchServer server = SearchServer(content);

        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "��������� ������� �������"s, DocumentStatus::ACTUAL, { 9 });

        vector<Document> founded = server.FindTopDocuments("�������� ��������� ���", DocumentStatus::ACTUAL);

        double max_rel = 10.0;
        int count = 0;
        for (auto found : founded) {
            if (found.relevance <= max_rel || abs(found.relevance - max_rel) < 1e-6) {
                max_rel = found.relevance;
                ++count;
            }
        }
        if (count == founded.size()) {
            ASSERT_HINT(true, Hint);
        }
        else {
            ASSERT_HINT(true, Hint);
        }
    }
    {
        string hint = "��������� � ���������� �������������� ������ ���� ������������� �� �������� � ������� ��������"s;
        const string content = "�"s;
        SearchServer server = SearchServer(content);

        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::IRRELEVANT, { 10 });
        server.AddDocument(1, "����� ��� � ������ �������"s, DocumentStatus::IRRELEVANT, { 20 });
        server.AddDocument(3, "����� ��� � ������ �������"s, DocumentStatus::IRRELEVANT, { 30 });
        server.AddDocument(4, "����� ��� � ������ �������"s, DocumentStatus::IRRELEVANT, { 40 });
        server.AddDocument(5, ""s, DocumentStatus::ACTUAL, { 9 });

        vector<Document> founded = server.FindTopDocuments("����� ��� � ������ �������", DocumentStatus::IRRELEVANT);

        ASSERT_EQUAL(founded.size(), 4);
        ASSERT_EQUAL_HINT(founded.at(0).rating, 40, hint);
        ASSERT_EQUAL_HINT(founded.at(1).rating, 30, hint);
        ASSERT_EQUAL_HINT(founded.at(2).rating, 20, hint);
        ASSERT_EQUAL_HINT(founded.at(3).rating, 10, hint);
    }
}

void TestRating() {
    const string Hint = "������� ����������� �� ���������";
    {
        const string content = "� ��"s;
        SearchServer server = SearchServer(content);

        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 0, 0, 0 });
        server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { -50, -10, 9 });
        server.AddDocument(2, "��������� ������� �������"s, DocumentStatus::ACTUAL, {});

        vector<Document> founded = server.FindTopDocuments("�������� ��������� ���", DocumentStatus::ACTUAL);

        ASSERT_EQUAL_HINT(founded[0].rating, -17, Hint);
        ASSERT_EQUAL_HINT(founded[1].rating, 0, Hint);
        ASSERT_EQUAL_HINT(founded[2].rating, 0, Hint);
    }
    {
        const string content = "� ��"s;
        SearchServer server = SearchServer(content);

        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 0, 200, 200 });
        server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { -50, -10, -9, -8, -1000, -9000 });
        server.AddDocument(2, "��������� ������� �������"s, DocumentStatus::ACTUAL, { 90000, 90000, 90000 });

        vector<Document> founded = server.FindTopDocuments("�������� ��������� ���", DocumentStatus::ACTUAL);

        ASSERT_EQUAL_HINT(founded[0].rating, -1679, Hint);
        ASSERT_EQUAL_HINT(founded[1].rating, 90000, Hint);
        ASSERT_EQUAL_HINT(founded[2].rating, 133, Hint);
    }
}

void TestPredicate() {
    const string content = "� ��"s;
    SearchServer search_server = SearchServer(content);

    search_server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "��������� ������� �������"s, DocumentStatus::BANNED, { 9 });

    {
        auto predicate = [](int document_id, DocumentStatus status,
            int rating) { return document_id % 2 == 0; };
        vector<Document> founded = search_server.FindTopDocuments("�������� ��������� ���"s, predicate);

        for (auto found : founded) {
            if (found.id % 2 != 0) {
                ASSERT(false);
            }
        }
    }
    {
        auto predicate = [](int document_id, DocumentStatus status,
            int rating) { return rating > 0; };
        vector<Document> founded = search_server.FindTopDocuments("�������� ��������� ���"s, predicate);

        for (auto found : founded) {
            if (found.rating <= 0) {
                ASSERT(false);
            }
        }
    }

}

void TestStatus() {
    {
        const string content = "� ��"s;
        SearchServer server = SearchServer(content);
        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 0, 0, 0 });
        server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { -50, -10, 9 });
        server.AddDocument(2, "��������� ������� �������"s, DocumentStatus::ACTUAL, {});

        vector<Document> founded = server.FindTopDocuments("�������� ��������� ���", DocumentStatus::BANNED);

        ASSERT(founded.empty());
    }
    {

        const string content = "� ��"s;
        SearchServer server = SearchServer(content);
        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::BANNED, { 0, 0, 0 });
        server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::BANNED, { -50, -10, 9 });
        server.AddDocument(2, "��������� ������� �������"s, DocumentStatus::BANNED, {});

        vector<Document> founded = server.FindTopDocuments("�������� ��������� ���", DocumentStatus::BANNED);

        ASSERT_EQUAL(founded.size(), 3);
    }
}

void TestIDF_TF() {
    {
        const string content = "�"s;
        SearchServer server = SearchServer(content);

        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 9 });

        vector<Document> founded = server.FindTopDocuments("����� ��� � ������ �������", DocumentStatus::ACTUAL);

        ASSERT_EQUAL(founded.at(0).relevance, 0.0);
        ASSERT_EQUAL(founded.at(1).relevance, 0.0);
        ASSERT_EQUAL(founded.at(2).relevance, 0.0);
    }
    {
        const string content = "� ��"s;
        SearchServer server = SearchServer(content);

        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        server.AddDocument(3, "��������� ������� �������"s, DocumentStatus::ACTUAL, { 9 });

        vector<Document> founded = server.FindTopDocuments("�������� ��������� ���");

        ASSERT_EQUAL(founded.at(0).relevance, 0.86643397569993164);
        ASSERT_EQUAL(founded.at(1).relevance, 0.23104906018664842);
        ASSERT_EQUAL(founded.at(2).relevance, 0.17328679513998632);
        ASSERT_EQUAL(founded.at(3).relevance, 0.17328679513998632);
    }
    {
        const string content = "��������� �� ������������� �����"s;
        SearchServer server = SearchServer(content);

        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        server.AddDocument(3, "��������� ������� �������"s, DocumentStatus::ACTUAL, { 9 });

        vector<Document> founded = server.FindTopDocuments("-�������� ��������� ���");

        ASSERT_EQUAL(founded.at(0).relevance, 0.13862943611198905);
    }
    {
        const string content = "�"s;
        SearchServer server = SearchServer(content);

        server.AddDocument(0, "����� � ������ ������� �������"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(2, "��������� �� ������������� ����� ����� �����"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        server.AddDocument(3, "������� �������"s, DocumentStatus::ACTUAL, { 9 });

        vector<Document> founded = server.FindTopDocuments("������� ������� �� �� �������");

        ASSERT_EQUAL(founded.at(0).relevance, 0.54930614433405489);
        ASSERT_EQUAL(founded.at(1).relevance, 0.54930614433405489);
        ASSERT_EQUAL(founded.at(2).relevance, 0.18310204811135161);
    }
    {
        const string content = " "s;
        SearchServer server = SearchServer(content);

        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::BANNED, { 8, -3 });
        server.AddDocument(1, "����� ��� � ������ �������"s, DocumentStatus::BANNED, { 7, 2, 7 });
        server.AddDocument(2, "����� ��� � ������ �������"s, DocumentStatus::BANNED, { 9 });

        vector<Document> founded = server.FindTopDocuments("����� ���", DocumentStatus::BANNED);

        ASSERT_EQUAL(founded.at(0).relevance, 0.0);
        ASSERT_EQUAL(founded.at(1).relevance, 0.0);
        ASSERT_EQUAL(founded.at(2).relevance, 0.0);
    }
}

void TestSearch() {
    {
        const string content = "� ��"s;
        SearchServer server = SearchServer(content);

        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        server.AddDocument(3, "��������� ������� �������"s, DocumentStatus::ACTUAL, { 9 });

        vector<Document> founded = server.FindTopDocuments("");

        ASSERT(founded.empty());
    }
    {
        const string content = "� ��"s;
        SearchServer server = SearchServer(content);

        vector<Document> founded = server.FindTopDocuments("�������� ��������� ���");

        ASSERT(founded.empty());
    }
    {
        const string content = "  "s;
        SearchServer server = SearchServer(content);

        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "-�������� ��� -�������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "____ -"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        server.AddDocument(3, "    "s, DocumentStatus::ACTUAL, { 9 });

        vector<Document> founded = server.FindTopDocuments("    ");

        ASSERT(founded.empty());
    }
}

void TestDocumentCount() {
    {
        const string content = "� ��"s;
        SearchServer server = SearchServer(content);

        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        server.AddDocument(3, "��������� ������� �������"s, DocumentStatus::ACTUAL, { 9 });

        ASSERT_EQUAL(server.GetDocumentCount(), 4);
    }
    {
        const string content = "����� ��� � ������ �������"s;
        SearchServer server = SearchServer(content);

        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, ""s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, ""s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        server.AddDocument(3, ""s, DocumentStatus::ACTUAL, { 9 });

        ASSERT_EQUAL(server.GetDocumentCount(), 4);
    }
}

void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatchedDocuments);
    RUN_TEST(TestSort);
    RUN_TEST(TestRating);
    RUN_TEST(TestPredicate);
    RUN_TEST(TestStatus);
    RUN_TEST(TestIDF_TF);
    RUN_TEST(TestSearch);
    RUN_TEST(TestDocumentCount);
}