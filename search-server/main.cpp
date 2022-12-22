#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

//#include "search_server.h"

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

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

        } else {
            word += c;
        }
    }

    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

int ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);

    return rating_sum / static_cast<int>(ratings.size());
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

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();

        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }

        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template<typename DocPre>
    vector<Document> FindTopDocuments(const string& raw_query, DocPre doc_pre) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, doc_pre);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });

        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query,
	        DocumentStatus requested_status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query,
                [&requested_status](int document_id, DocumentStatus status, int rating) {
                    return status == requested_status;
                });
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const {
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

        return {matched_words, documents_.at(document_id).status};
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

        return {text, is_minus, IsStopWord(text)};
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
                } else {
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

    template<typename DocPre>
    vector<Document> FindAllDocuments(const Query& query, DocPre doc_pre) const {
        map<int, double> document_to_relevance;
        vector<Document> matched_documents;

        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }

            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            	const auto& doc_status_rating = documents_.at(document_id);

            	if (doc_pre(document_id, doc_status_rating.status, doc_status_rating.rating)) {
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

        for (const auto [document_id, relevance] : document_to_relevance) {
	    matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
        }

        return matched_documents;
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

// Реализация макросов ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
template <typename Container>
void PrintContainer(ostream& out, const Container& container) {
	int first_element_check = 1;

	for (const auto& element : container) {
		if (first_element_check) {
			out << element;
			first_element_check -= 1;
		} else {
			out << ", "s << element;
		}
	}
}

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

template <typename Element>
ostream& operator<<(ostream& out, const vector<Element>& container) {
	out << "["s;
	PrintContainer(out, container);
	out << "]"s;
	return out;
}

ostream& operator<<(ostream& out, const DocumentStatus& status) {
	out << "["s;
	PrintDocumentStatus(out, status);
	out << "]"s;
	return out;
}

template <typename Function>
void RunTestImpl(const Function& test_func, const string& test_func_str) {
    test_func();

    cerr << test_func_str << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str,
                     const string& file, const string& func, unsigned line, const string& hint) {
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

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;

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

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

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
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

// Тест проверяет, что добавленный документ находтся по посковому запросу,
// который содержит слова из документа
void TestSearchingAddDocument() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = {1, 2, 3};

	{
		SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto found_docs = server.FindTopDocuments("Birds and one cat in the town"s);
		ASSERT_EQUAL_HINT(found_docs.size(), 1u,
				  "In this case, the function should return one document"s);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id);
	}

	{
		SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT_HINT(server.FindTopDocuments("Birds and one dog are there"s).empty(),
		            "A query that does not contain word(s) from the document should not push it"s);
	}
}

// Тест проверяет, исключаются ли стоп-слова из текста документа
void TestDocumentsWithMinusWordsNotInResult() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = {1, 2, 3};

	{
		SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT_HINT(server.FindTopDocuments("dogs in the -city"s).empty(),
		            "A query that contains minus-word from the document should not push it"s);
	}

	{
		SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto found_docs = server.FindTopDocuments("dogs in the city -town"s);
		ASSERT_EQUAL(found_docs.size(), 1u);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id);
	}
}

// Тест проверяет, ссответствуют ли документы поисковому запросу;
// должны возвращаться все слова из поискового запроса;
// при соответствии по минус-слову, должен возвращаться пустой список
void TestDocumentsMatching() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = {1, 2, 3};

	{
		SearchServer server;
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
		SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto matching_doc = server.MatchDocument("big dog in the city -cat"s, doc_id);
		const vector<string> matching_words = get<0>(matching_doc);
		ASSERT_HINT(matching_words.empty(),
                            "There is a match for minus-word, vector matching_words should be empty"s);
	}
}

// Тест проверяет сортировку документов по релевантности,
// результаты должны быть отсортированы в порядке убывания
void TestRelevanceSorting() {
	const DocumentStatus status = DocumentStatus::ACTUAL;

	{
		SearchServer server;
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

// Тест проверяет расчет рэйтинга документов,
// рэйтинг равен среднему арифметическому оценок документа
void TestDocumentsRatingCalc() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const DocumentStatus status = DocumentStatus::ACTUAL;

	{
		SearchServer server;
		server.AddDocument(doc_id, content, status, {});
		const auto found_docs = server.FindTopDocuments("cat"s);
		ASSERT_EQUAL_HINT(found_docs[0].rating, 0,
				  "In this case, the expected rating is 0");
	}

	{
		SearchServer server;
		server.AddDocument(doc_id, content, status, {7, 2, 7});
		const auto found_docs = server.FindTopDocuments("cat"s);
		// (7 + 2 + 7) / 3 = 5.(3) ~ отбрасываем дробную часть
		ASSERT_EQUAL_HINT(found_docs[0].rating, 5,
                                  "In this case, the expected rating is 5");
	}

	{
		SearchServer server;
		server.AddDocument(doc_id, content, status, {-12, -20, 3});
		const auto found_docs = server.FindTopDocuments("cat"s);
		// (-12 - 20 + 3) / 3 = -9.(6) ~ отбрасываем дробную часть
		ASSERT_EQUAL_HINT(found_docs[0].rating, -9,
                                  "In this case, the expected rating is -9");
	}
}

// Тест проверяет фильтрацию результатов с использованием предиката
void TestPredicatFucntionInFindTopDocuments() {
	{
		SearchServer server;
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
		SearchServer server;
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
		SearchServer server;
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

// Тест проверяет поиск документов с заданным статусом
void TestFindTopDocumentsFuncWithStatus() {
	{
		SearchServer server;
		server.AddDocument(0, "fluffy cat beautiful dog"s, DocumentStatus::ACTUAL, {8, -3});
		server.AddDocument(1, "angry cat angry dog"s, DocumentStatus::IRRELEVANT, {7, 2, 7});
		server.AddDocument(2, "dog pretty eyes"s, DocumentStatus::BANNED, {5, -12, 2, 1});
		server.AddDocument(3, "crazy bird"s, DocumentStatus::REMOVED, {9});
		const auto found_doc_actual = server.FindTopDocuments("angry crazy cat with eyes"s,
                                                                      DocumentStatus::ACTUAL);
		ASSERT_EQUAL_HINT(found_doc_actual[0].id, 0,
						  "Wrong id, expected id is 0"s);
		ASSERT_EQUAL_HINT(found_doc_actual.size(), 1u,
						  "There should be one actual status document"s);

		const auto found_doc_irrelevant = server.FindTopDocuments("angry crazy cat with eyes"s,
                                                                          DocumentStatus::IRRELEVANT);
		ASSERT_EQUAL_HINT(found_doc_irrelevant[0].id, 1,
						  "Wrong id, expected id is 1"s);
		ASSERT_EQUAL_HINT(found_doc_irrelevant.size(), 1u,
						  "There should be one irrelevant status document"s);

		const auto found_doc_banned = server.FindTopDocuments("angry crazy cat with eyes"s,
                                                                      DocumentStatus::BANNED);
		ASSERT_EQUAL_HINT(found_doc_banned[0].id, 2,
						  "Wrong id, expected id is 2"s);
		ASSERT_EQUAL_HINT(found_doc_banned.size(), 1u,
						  "There should be one banned status document"s);

		const auto found_doc_removed = server.FindTopDocuments("angry crazy cat with eyes"s,
                                                                       DocumentStatus::REMOVED);
		ASSERT_EQUAL_HINT(found_doc_removed[0].id, 3,
						  "Wrong id, expected id is 3"s);
		ASSERT_EQUAL_HINT(found_doc_removed.size(), 1u,
						  "There should be one removed status document"s);
	}
}

// Тест проверяет корректность вычислений релеванстонти документов
void TestDocumentsRelevanceCalc() {
	const DocumentStatus status = DocumentStatus::ACTUAL;

	{
		SearchServer server;
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

// Функция TestSearchServer является точкой входа для запуска тестов
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
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;

    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                                                   DocumentStatus::BANNED)) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s,
            [](int document_id, DocumentStatus status, int rating) {
    		    return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
}
