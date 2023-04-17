#include "document.h"
#include "log_duration.h"
#include "paginator.h"
#include "process_queries.h"
#include "read_input_functions.h"
#include "remove_duplicates.h"
#include "request_queue.h"
#include "search_server.h"
#include "string_processing.h"
#include "test_example_functions.h"

using namespace std;

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;

    SearchServer search_server("and with"s);
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
    const vector<string> queries = {
        "nasty rat -not"s,
        "not very funny nasty pet"s,
        "curly hair"s
    };
    for (const Document& document : ProcessQueriesJoined(search_server, queries)) {
        cout << "Document "s << document.id << " matched with relevance "s
             << document.relevance << endl;
    }

    MatchDocuments(search_server, "pet curly"s);

    return 0;
}
