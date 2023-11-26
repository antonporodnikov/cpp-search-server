# SearchServer

Система поиска документов по ключевым словам. Программа включает в себя ранжирование результатов поиска по показателю TF-IDF, обработку стоп-слов и минус-слов, создание и обработку очереди запросов, удаление дубликатов документов, постраничное разделение результатов поиска, возможность работы в многопоточном режиме.

### Работа с программой
- Создается экземпляр класса SerachServer с возможностью добавления стоп-слов и минус-слов.
- Метод AddDocument позволяет добавлять документы для поиска.
- Метод FindTopDocuments возвращает результат.
- С помощью класса RequestQueue можно реализовать очередь запросов.

### Системные требования
Компилятор С++ с поддержкой стандарта C++17 или новее.

### Сборка
Любой терминал, эмулятор терминала, консоль, командная строка.

### Планы по доработке
Добавление графического интерфейса.

# cpp-search-server
