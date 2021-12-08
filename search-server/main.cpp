// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь:
#include <iostream>
#include <vector>
#include <string>

using namespace std;

int main() {
	vector<string> test;
	int counter = 0;
	for (int i = 1; i <= 1000; ++i) {
		test.push_back(to_string(i));
	}
	for (string s : test) {
		for (char c : s) {
			if (c == '3') {
				++counter;
				break;
			}
		}
	}
	cout << counter;
}
// Закомитьте изменения и отправьте их в свой репозиторий.
