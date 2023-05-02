#pragma once
#include <algorithm>
#include <cmath>
#include <deque>
#include <execution>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

#include "concurrent_map.h"
#include "document.h"
#include "string_processing.h"

using namespace std::string_literals;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;
const int CONCURRENT_MAP_BUCKETS_AMOUNT = 32;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text);

    explicit SearchServer(std::string_view stop_words_text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status,
                     const std::vector<int>& ratings);

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy,
                                           std::string_view raw_query,
                                           DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query,
                                           DocumentPredicate document_predicate) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy,
                                           std::string_view raw_query,
                                           DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query,
                                           DocumentStatus status) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy,
                                           std::string_view raw_query) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    int GetDocumentCount() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus>
    MatchDocument(std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
        const std::execution::sequenced_policy&,
        std::string_view raw_query,
        int document_id
    ) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
        const std::execution::parallel_policy& policy,
        std::string_view raw_query,
        int document_id
    ) const;

    void RemoveDocument(int document_id);

    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);

    void RemoveDocument(const std::execution::parallel_policy& policy, int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const std::set<std::string, std::less<>> stop_words_;
    std::deque<std::string> document_to_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> id_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(std::string_view word) const;

    static bool IsValidWord(std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(std::string_view text) const;

    Query ParseQueryPar(std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&,
                                           const Query& query,
                                           DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&,
                                           const Query& query,
                                           DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
    {
        if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy,
                                                     std::string_view raw_query,
                                                     DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
                return lhs.rating > rhs.rating;
            }
            return lhs.relevance > rhs.relevance;
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
                                                     DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy,
                                                     std::string_view raw_query,
                                                     DocumentStatus status) const {
    return FindTopDocuments(
        policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy,
                                                     std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&,
                                                     const Query& query,
                                                     DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    {
        for (std::string_view word : query.plus_words) {
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
    }
    {
        for (std::string_view word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
    }
    std::vector<Document> matched_documents;
    {
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&,
                                                     const Query& query,
                                                     DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(CONCURRENT_MAP_BUCKETS_AMOUNT);
    std::vector<std::string_view> words_in_documents;
    std::copy_if(std::execution::par, query.plus_words.begin(), query.plus_words.end(),
        back_inserter(words_in_documents), [this](std::string_view plus_word) {
            return word_to_document_freqs_.count(plus_word);
        });
    std::for_each(std::execution::par, words_in_documents.begin(), words_in_documents.end(),
        [this, &document_to_relevance, document_predicate](std::string_view word) {
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id].ref_to_value += (
                        term_freq * inverse_document_freq);
                }
            }
        });
    std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
        [this, &document_to_relevance](std::string_view word) {
            if (word_to_document_freqs_.count(word) != 0) {
                for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                    document_to_relevance.Erase(document_id);
                }
            }
        });
        
    std::map<int, double> document_to_relevance_map;
    document_to_relevance_map = document_to_relevance.BuildOrdinaryMap();
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance_map) {
        matched_documents.push_back(
            {document_id, relevance, documents_.at(document_id).rating});
    }

    return matched_documents;
}

void AddDocument(SearchServer& search_server, int document_id, const std::string& document,
                 DocumentStatus status, const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string& query);
