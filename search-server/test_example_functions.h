#pragma once
#include <iostream>

#include "document.h"
#include "search_server.h"

#define RUN_TEST(func) RunTestImpl((func), #func)

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

using namespace std::string_literals;

template <typename Container>
void PrintContainer(std::ostream& out, const Container& container) {
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

void PrintDocumentStatus(std::ostream& out, const DocumentStatus& status);

template <typename Element>
std::ostream& operator<<(std::ostream& out, const std::vector<Element>& container) {
    out << "["s;
    PrintContainer(out, container);
    out << "]"s;
    return out;
}

std::ostream& operator<<(std::ostream& out, const DocumentStatus& status);

template <typename Function>
void RunTestImpl(const Function& test_func, const std::string& test_func_str) {
    test_func();
    std::cerr << test_func_str << " OK"s << std::endl;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str,
		             const std::string& u_str, const std::string& file,
					 const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cerr << std::boolalpha;
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

void AssertImpl(bool value, const std::string& expr_str, const std::string& file,
		        const std::string& func, unsigned line, const std::string& hint);

void TestExcludeStopWordsFromAddedDocumentContent();

void TestSearchingAddDocument();

void TestDocumentsWithMinusWordsNotInResult();

void TestDocumentsMatching();

void TestRelevanceSorting();

void TestDocumentsRatingCalc();

void TestPredicatFucntionInFindTopDocuments();

void TestFindTopDocumentsFuncWithStatus();

void TestDocumentsRelevanceCalc();

void TestSearchServer();
