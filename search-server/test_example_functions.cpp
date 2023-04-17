#include "test_example_functions.h"

using namespace std;

void PrintDocumentStatus(ostream& out, const DocumentStatus& status) {
    switch (status) {
    case DocumentStatus::ACTUAL:
        out << "DocumentStatus::ACTUAL";
        break;
    case DocumentStatus::IRRELEVANT:
        out << "DocumentStatus::IRRELEVANT";
        break;
    case DocumentStatus::BANNED:
        out << "DocumentStatus::BANNED";
        break;
    case DocumentStatus::REMOVED:
        out << "DocumentStatus::REMOVED";
        break;
    default:
        out << "Unknown DocumentStatus";
    }
}

ostream& operator<<(ostream& out, const DocumentStatus& status) {
    out << "["s;
    PrintDocumentStatus(out, status);
    out << "]"s;
    return out;
}

void AssertImpl(bool value, const string& expr_str, const string& file,
                const string& func, unsigned line, const string& hint) {
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
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server("empty"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

void TestSearchingAddDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server("empty"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("Birds and one cat in the town"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
                          "In this case, the function should return one document"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        SearchServer server("empty"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("Birds and one dog are there"s).empty(),
                    "A query that does not contain word(s) from the document should not push it"s);
    }
}

void TestDocumentsWithMinusWordsNotInResult() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server("empty"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("dogs in the -city"s).empty(),
                    "A query that contains minus-word from the document should not push it"s);
    }
    {
        SearchServer server("empty"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("dogs in the city -town"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
}

void TestDocumentsMatching() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server("empty"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto matching_doc = server.MatchDocument("big cat and dog in the city"s, doc_id);
        vector<string> content_words = SplitIntoWords(content);
        vector<string> matching_words = get<0>(matching_doc);
        sort(content_words.begin(), content_words.end());
        sort(matching_words.begin(), matching_words.end());
        ASSERT_EQUAL(content_words, matching_words);
        const DocumentStatus matching_doc_status = get<1>(matching_doc);
        ASSERT_EQUAL(matching_doc_status, DocumentStatus::ACTUAL);
    }
    {
        SearchServer server("empty"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto matching_doc = server.MatchDocument("big dog in the city -cat"s, doc_id);
        const vector<string> matching_words = get<0>(matching_doc);
        ASSERT_HINT(matching_words.empty(),
                    "There is a match for minus-word, vector matching_words should be empty"s);
    }
}

void TestRelevanceSorting() {
    const DocumentStatus status = DocumentStatus::ACTUAL;
    {
        SearchServer server("empty"s);
        // Релевантность документа 0.173287
        server.AddDocument(0, "fluffy cat beautiful dog"s, status, {8, -3});
        // Релевантность документа 0.866434
        server.AddDocument(1, "angry cat angry dog"s, status, {7, 2, 7});
        // Релевантность документа 0.462098
        server.AddDocument(2, "dog pretty eyes"s, status, {5, -12, 2, 1});
        // Релевантность документа 0.693147
        server.AddDocument(3, "crazy bird"s, status, {9});
        const auto found_docs = server.FindTopDocuments("angry crazy cat with eyes"s);
        ASSERT_HINT(found_docs[0].relevance >= found_docs[1].relevance &&
                    found_docs[1].relevance >= found_docs[2].relevance &&
                    found_docs[2].relevance >= found_docs[3].relevance,
                    "Sorting by relevance is not correct, sorting should be in descending order"s);
    }
}

void TestDocumentsRatingCalc() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const DocumentStatus status = DocumentStatus::ACTUAL;
    {
        SearchServer server("empty"s);
        server.AddDocument(doc_id, content, status, {});
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL_HINT(found_docs[0].rating, 0,
                          "In this case, the expected rating is 0");
    }
    {
        SearchServer server("empty"s);
        server.AddDocument(doc_id, content, status, {7, 2, 7});
        const auto found_docs = server.FindTopDocuments("cat"s);
        // (7 + 2 + 7) / 3 = 5.(3) ~ отбрасываем дробную часть
        ASSERT_EQUAL_HINT(found_docs[0].rating, 5,
                          "In this case, the expected rating is 5");
    }
    {
        SearchServer server("empty"s);
        server.AddDocument(doc_id, content, status, {-12, -20, 3});
        const auto found_docs = server.FindTopDocuments("cat"s);
        // (-12 - 20 + 3) / 3 = -9.(6) ~ отбрасываем дробную часть
        ASSERT_EQUAL_HINT(found_docs[0].rating, -9,
                          "In this case, the expected rating is -9");
    }
}

void TestPredicatFucntionInFindTopDocuments() {
    {
        SearchServer server("empty"s);
        server.AddDocument(0, "fluffy cat beautiful dog"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "angry cat angry dog"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(2, "dog pretty eyes"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        server.AddDocument(3, "crazy bird"s, DocumentStatus::ACTUAL, {9});
        const auto found_docs = server.FindTopDocuments("angry crazy cat with eyes"s,
                [](int document_id, DocumentStatus status, int rating) {
                	return document_id % 2 == 0; });
        ASSERT_EQUAL_HINT(found_docs.size(), 2u,
                          "In this case, the size of the found_docs should be equal to 2"s);
        ASSERT_EQUAL_HINT(found_docs[0].id, 2,
                          "In this case, first document id should be equal to 2"s);
        ASSERT_EQUAL_HINT(found_docs[1].id, 0,
                          "In this case, second document id should be equal to 0"s);
    }
    {
        SearchServer server("empty"s);
        server.AddDocument(0, "fluffy cat beautiful dog"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "angry cat angry dog"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(2, "dog pretty eyes"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        server.AddDocument(3, "crazy bird"s, DocumentStatus::ACTUAL, {9});
        const auto found_docs = server.FindTopDocuments("angry crazy cat with eyes"s,
                [](int document_id, DocumentStatus status, int rating) {
                    return document_id < 0; });
        ASSERT_HINT(found_docs.empty(),
                    "In this case, found_docs should not contain documents"s);
    }
    {
        SearchServer server("empty"s);
        server.AddDocument(0, "fluffy cat beautiful dog"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "angry cat angry dog"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(2, "dog pretty eyes"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        server.AddDocument(3, "crazy bird"s, DocumentStatus::ACTUAL, {9});
        const auto found_docs = server.FindTopDocuments("angry crazy cat with eyes"s,
                [](int document_id, DocumentStatus status, int rating) {
                    return rating < 0; });
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
                          "in this case, only one document has a rating less than zero"s);
        ASSERT_EQUAL_HINT(found_docs[0].id, 2,
                          "In this case, document's id should be equal to 2"s);
    }
}

void TestFindTopDocumentsFuncWithStatus() {
    {
        SearchServer server("empty"s);
        server.AddDocument(0, "fluffy cat beautiful dog"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "angry cat angry dog"s, DocumentStatus::IRRELEVANT, {7, 2, 7});
        server.AddDocument(2, "dog pretty eyes"s, DocumentStatus::BANNED, {5, -12, 2, 1});
        server.AddDocument(3, "crazy bird"s, DocumentStatus::REMOVED, {9});
        const auto found_doc_actual = server.FindTopDocuments("angry crazy cat with eyes"s,
                                                              DocumentStatus::ACTUAL);
        ASSERT_EQUAL_HINT(found_doc_actual[0].id, 0, "Wrong id, expected id is 0"s);
        ASSERT_EQUAL_HINT(found_doc_actual.size(), 1u,
                          "There should be one actual status document"s);

        const auto found_doc_irrelevant = server.FindTopDocuments("angry crazy cat with eyes"s,
                                                                  DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL_HINT(found_doc_irrelevant[0].id, 1, "Wrong id, expected id is 1"s);
        ASSERT_EQUAL_HINT(found_doc_irrelevant.size(), 1u,
                          "There should be one irrelevant status document"s);

        const auto found_doc_banned = server.FindTopDocuments("angry crazy cat with eyes"s,
                                                              DocumentStatus::BANNED);
        ASSERT_EQUAL_HINT(found_doc_banned[0].id, 2, "Wrong id, expected id is 2"s);
        ASSERT_EQUAL_HINT(found_doc_banned.size(), 1u,
                          "There should be one banned status document"s);

        const auto found_doc_removed = server.FindTopDocuments("angry crazy cat with eyes"s,
                                                               DocumentStatus::REMOVED);
        ASSERT_EQUAL_HINT(found_doc_removed[0].id, 3, "Wrong id, expected id is 3"s);
        ASSERT_EQUAL_HINT(found_doc_removed.size(), 1u,
                          "There should be one removed status document"s);
    }
}

void TestDocumentsRelevanceCalc() {
    const DocumentStatus status = DocumentStatus::ACTUAL;
    {
        SearchServer server("empty"s);
        server.AddDocument(0, "fluffy cat beautiful dog"s, status, {8, -3});
        double id0_doc_relevance = 0.173287;
        server.AddDocument(1, "angry cat angry dog"s, status, {7, 2, 7});
        double id1_doc_relevance = 0.866434;
        server.AddDocument(2, "dog pretty eyes"s, status, {5, -12, 2, 1});
        double id2_doc_relevance = 0.462098;
        server.AddDocument(3, "crazy bird"s, status, {9});
        double id3_doc_relevance = 0.693147;
        const auto found_docs = server.FindTopDocuments("angry crazy cat with eyes"s);
        ASSERT_HINT(abs(found_docs[0].relevance - id1_doc_relevance) < EPSILON,
                    "Wrong relevance, should be 0.866434 for this document");
        ASSERT_HINT(abs(found_docs[1].relevance - id3_doc_relevance) < EPSILON,
                    "Wrong relevance, should be 0.866434 for this document");
        ASSERT_HINT(abs(found_docs[2].relevance - id2_doc_relevance) < EPSILON,
                    "Wrong relevance, should be 0.866434 for this document");
        ASSERT_HINT(abs(found_docs[3].relevance - id0_doc_relevance) < EPSILON,
                    "Wrong relevance, should be 0.866434 for this document");
    }
}

void TestProcessQueries() {
    {
        SearchServer server("and with"s);
        int id = 0;
        for (
            const string& text : {
                "funny pet and nasty rat"s,
                "funny pet with curly hair"s,
                "funny pet and not very nasty rat"s,
                "pet with rat and rat and rat"s,
                "nasty rat with curly hair"s,
            }
        ) {
            server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
        }
        const vector<string> queries = {
            "nasty rat -not"s,
            "not very funny nasty pet"s,
            "curly hair"s
        };
        auto result = ProcessQueriesJoined(server, queries);
        ASSERT_EQUAL_HINT(result.size(), 10, 
                          "Wrong amount of documents, should be 10"s);
        ASSERT_EQUAL_HINT(result.back().id, 5,
                          "Wrong document added, id should be 5 for this document"s);
        result.pop_back();
        ASSERT_EQUAL_HINT(result.back().id, 2,
                          "Wrong document added, id should be 2 for this document"s);
        result.pop_back();
        ASSERT_EQUAL_HINT(result.back().id, 4,
                          "Wrong document added, id should be 4 for this document"s);
        result.pop_back();
        ASSERT_EQUAL_HINT(result.back().id, 5,
                          "Wrong document added, id should be 5 for this document"s);
        result.pop_back();
        ASSERT_EQUAL_HINT(result.back().id, 2,
                          "Wrong document added, id should be 2 for this document"s);
        result.pop_back();
        ASSERT_EQUAL_HINT(result.back().id, 1,
                          "Wrong document added, id should be 1 for this document"s);
        result.pop_back();
        ASSERT_EQUAL_HINT(result.back().id, 3,
                          "Wrong document added, id should be 3 for this document"s);
        result.pop_back();
        ASSERT_EQUAL_HINT(result.back().id, 4,
                          "Wrong document added, id should be 4 for this document"s);
        result.pop_back();
        ASSERT_EQUAL_HINT(result.back().id, 5,
                          "Wrong document added, id should be 5 for this document"s);
        result.pop_back();
        ASSERT_EQUAL_HINT(result.back().id, 1,
                          "Wrong document added, id should be 1 for this document"s);
    }
}

void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestSearchingAddDocument);
    RUN_TEST(TestDocumentsWithMinusWordsNotInResult);
    RUN_TEST(TestDocumentsMatching);
    RUN_TEST(TestRelevanceSorting);
    RUN_TEST(TestDocumentsRatingCalc);
    RUN_TEST(TestPredicatFucntionInFindTopDocuments);
    RUN_TEST(TestFindTopDocumentsFuncWithStatus);
    RUN_TEST(TestDocumentsRelevanceCalc);
    RUN_TEST(TestProcessQueries);
}
