#include "process_queries.h"

using namespace std;

vector<vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const vector<string>& queries) {
        vector<vector<Document>> result(queries.size());
        transform(execution::par, queries.begin(), queries.end(), result.begin(),
                  [&search_server](const string& query) {
                      return search_server.FindTopDocuments(query);
                  });
        
    return result;
}

list<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const vector<string>& queries) {
        list<Document> result;
        vector<vector<Document>> result_unjoined = ProcessQueries(search_server, queries);
        for (const vector<Document>& documents : result_unjoined) {
            result.insert(result.end(), documents.begin(), documents.end());
        }

        return result;
}
