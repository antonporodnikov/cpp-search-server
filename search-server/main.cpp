#include "document.h"
#include "log_duration.h"
#include "paginator.h"
#include "process_queries.h"
#include "read_input_functions.h"
// #include "remove_duplicates.h"
#include "request_queue.h"
#include "search_server.h"
#include "string_processing.h"
// #include "test_example_functions.h"

using namespace std;

int main() {
    // TestSearchServer();
    // // Если вы видите эту строку, значит все тесты прошли успешно
    // cout << "Search server testing finished"s << endl;

    // SearchServer search_server("and with"s);

    string_view stop_words = "and with";
    SearchServer search_server(stop_words);

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
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }

    FindTopDocuments(search_server, "funny curly"s);

    MatchDocuments(search_server, "funny curly"s);

    const string query = "curly and funny -not"s;

    {
        const auto [words, status] = search_server.MatchDocument(query, 1);
        cout << words.size() << " words for document 1"s << endl;
        // 1 words for document 1
    }

    {
        const auto [words, status] = search_server.MatchDocument(execution::seq, query, 2);
        cout << words.size() << " words for document 2"s << endl;
        // 2 words for document 2
    }

    {
        const auto [words, status] = search_server.MatchDocument(execution::par, query, 3);
        cout << words.size() << " words for document 3"s << endl;
        // 0 words for document 3
    }

    cout << search_server.GetDocumentCount() << endl;

    search_server.RemoveDocument(execution::par, 2);

    cout << search_server.GetDocumentCount() << endl;

    cout << "[ program finished successfully ]"s << endl;

    return 0;
}
