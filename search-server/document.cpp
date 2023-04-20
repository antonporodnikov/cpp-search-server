#include "document.h"

using namespace std;

ostream& operator<<(ostream& out, const Document& document) {
    out << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s;
    return out;
}

void PrintMatchDocumentResult(int document_id, vector<string_view> words,
                              DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (string_view word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}
