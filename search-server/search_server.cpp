#include "search_server.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    // Invoke delegating constructor from string container
    : SearchServer(SplitIntoWords(stop_words_text))
{
}

SearchServer::SearchServer(string_view stop_words_text)
    // Invoke delegating constructor from string container
    : SearchServer(SplitIntoWords(string(stop_words_text.begin(), stop_words_text.end())))
{
}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status,
                               const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id"s);
    }
    document_to_words_.emplace_back(document.begin(), document.end());
    vector<string_view> words = SplitIntoWordsNoStop(document_to_words_.back());
    const double inv_word_count = 1.0 / words.size();
    for (string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        id_to_word_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query,
                                                DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    auto it = id_to_word_freqs_.find(document_id);
    const static map<string_view, double> empty_map = {};
    if (it != id_to_word_freqs_.end()) {
        return (*it).second;
    }
    return empty_map;
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query,
                                                                       int document_id) const {
    const auto query = ParseQuery(raw_query);
    if (any_of(
        query.minus_words.begin(),
        query.minus_words.end(),
        [this, document_id](string_view minus_word) {
            return word_to_document_freqs_.count(minus_word) != 0 &&
            word_to_document_freqs_.at(minus_word).count(document_id);
        }
    )) {
        return {vector<string_view>{}, documents_.at(document_id).status};
    }
    vector<string_view> matched_words;
    for (string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(
    const execution::sequenced_policy&,
    string_view raw_query,
    int document_id
) const {
    return MatchDocument(raw_query, document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(
    const execution::parallel_policy& policy,
    string_view raw_query,
    int document_id
) const {
    const auto query = ParseQueryPar(raw_query);
    if (any_of(
        policy,
        query.minus_words.begin(),
        query.minus_words.end(),
        [this, document_id](string_view minus_word) {
            return word_to_document_freqs_.count(minus_word) != 0 &&
            word_to_document_freqs_.at(minus_word).count(document_id);
        }
    )) {
        return {vector<string_view>{}, documents_.at(document_id).status};
    }
    vector<string_view> matched_words(query.plus_words.size());
    auto it_last = copy_if(
        policy,
        query.plus_words.begin(),
        query.plus_words.end(),
        matched_words.begin(),
        [this, document_id](string_view plus_word) {
            return word_to_document_freqs_.count(plus_word) != 0 &&
            word_to_document_freqs_.at(plus_word).count(document_id);
        }
    );
    sort(policy, matched_words.begin(), it_last);
    auto it_last_new = unique(policy, matched_words.begin(), it_last);
    matched_words.erase(it_last_new, matched_words.end());
    return {matched_words, documents_.at(document_id).status};
}

void SearchServer::RemoveDocument(int document_id) {
    if (document_ids_.find(document_id) == document_ids_.end()) {
        return;
    }
    for (auto& [word, id_freq] : word_to_document_freqs_) {
        auto it = id_freq.find(document_id);
        if (it != id_freq.end()) {
            id_freq.erase(it);
        }
    }
    {
        auto it = documents_.find(document_id);
        if (it != documents_.end()) {
            documents_.erase(it);
        }
    }
    {
        auto it = find(document_ids_.begin(),
                       document_ids_.end(), document_id);
        document_ids_.erase(it);
    }
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id) {
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy& policy, int document_id) {
    if (document_ids_.find(document_id) == document_ids_.end()) {
        return;
    }
    vector<string_view> words_to_delete(id_to_word_freqs_.at(document_id).size());
    transform(policy, id_to_word_freqs_.at(document_id).begin(), 
              id_to_word_freqs_.at(document_id).end(), words_to_delete.begin(), 
              [](auto word_freq) {
                  return word_freq.first;
              });
    for_each(policy, words_to_delete.begin(), words_to_delete.end(),
             [this, document_id](string_view word) {
                 word_to_document_freqs_.at(word).erase(document_id);
             });
    {
        auto it = documents_.find(document_id);
        if (it != documents_.end()) {
            documents_.erase(it);
        }
    }
    {
        auto it = find(document_ids_.begin(),
                       document_ids_.end(), document_id);
        document_ids_.erase(it);
    }
}

bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(string_view word) {
    // A valid word must not contain special characters
    string word_str(word.begin(), word.end());
    return none_of(word_str.begin(), word_str.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string_view> words;
    for (string_view word : SplitIntoWordsSTRV(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + string(word.begin(), word.end()) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
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
        throw invalid_argument("Query word "s + string(text.begin(), text.end()) + " is invalid");
    }
    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
    Query result;
    for (string_view word : SplitIntoWordsSTRV(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            } else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    auto remove_duplicates = [](vector<string_view>& words) {
        sort(words.begin(), words.end());
        auto it_last = unique(words.begin(), words.end());
        words.erase(it_last, words.end());
    };
    remove_duplicates(result.minus_words);
    remove_duplicates(result.plus_words);
    return result;
}

SearchServer::Query SearchServer::ParseQueryPar(string_view text) const {
    Query result;
    for (string_view word : SplitIntoWordsSTRV(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            } else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

void AddDocument(SearchServer& search_server, int document_id, const string& document,
                 DocumentStatus status, const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const exception& e) {
        cout << "Error in adding document "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Results for request: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            cout << document << endl;;
        }
    } catch (const exception& e) {
        cout << "Error is seaching: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Matching for request: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        auto document_id_it = search_server.begin();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = *document_id_it++;
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "Error in matchig request "s << query << ": "s << e.what() << endl;
    }
}
