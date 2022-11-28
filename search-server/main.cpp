// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь: 271 число

// Закомитьте изменения и отправьте их в свой репозиторий.

#include <iostream>

using namespace std;

int main() {
    int res = 0;

    for (int i = 0; i < 1001; ++i) {
        for (char c : to_string(i)) {
            if (c == '3') {
                ++res;
                break;
            }
        }
    }

    cout << res << endl;

    return 0;
}
