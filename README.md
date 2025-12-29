# Команды запуска всех программ
Все команды выполняются относительно корневой директории проекта

## 0. Установка зависимостей

```bash
pip3 install -r requirements.txt
```

## 1. Поисковый робот

### Запуск робота для сбора документов
```bash
cd crawler
python3 crawler.py config.yaml
```

### Что делает робот:
- Скачивает страницы с сайтов согласно конфигурации
- Извлекает ссылки и добавляет их в очередь
- Сохраняет HTML в MongoDB
- Поддерживает возобновление после остановки
- Периодически переобкачивает устаревшие документы

## 2. Получение корпуса из MongoDB в txt

### Извлечение текста из HTML документов
```bash
cd utils
python3 prepare_tokenizer.py ../crawler/config.yaml ../data/corpus.txt
```

Результаты:
- data/corpus.txt - тексты документов (каждый документ отделен пустой строкой)
- data/corpus.txt.urls - URL документов в том же порядке
- data/corpus.txt.meta.json - статистика обработки

## 3. Сборка компонент поискового движка
```bash
make
```

## 4. Токенизация

### Запуск токенизации
```bash
cd tokenizer
./tokenizer.out ../data/corpus.txt
```
Результаты:
- tokenizer/tokens.txt - список токенов (один токен на строку)
- tokenizer/tokens.txt.stats.json - статистика токенизации

## 5. Стемминг 
### Запуск стемминга
```bash
cd stemmer
./stemmer.out ../tokenizer/tokens.txt
```
Результаты:
- stemmer/stemmed_tokens.txt - токены после стемминга
- stemmer/stemmed_tokens.txt.stats.json - статистика стемминга
- stemmer/stemming_analysis.json - анализ качества стемминга

### Анализ результатов стемминга
```bash
cd utils
python3 analyze_stemming.py ../tokenizer/tokens.txt ../stemmer/stemmed_tokens.txt
```

## 6. Закон Ципфа

### Построение распределения Ципфа
```bash
cd zipf
python3 zipf_law.py
```
Результаты сохраняются в zipf/results/:
- zipf_plot.png - график распределения Ципфа
- top_tokens.txt - топ-100 токенов по частотности
- zipf_statistics.json - статистика распределения

## 7. Булев индекс

### Построение булева индекса
```bash
cd search_engine
# без стемминга
./search_engine.out build ../data/corpus.txt index.bin

# со стеммингом
./search_engin.out build ../data/corpus.txt index_stemmed.bin --stemming
```

## 8. Булев поиск

### Запуск поиска
```bash
cd search_engine
# Поиск без стемминга
./search_engine.out search index.bin

# Поиск со стеммингом
./search_engine.out search index_stemmed.bin --stemming

# Поиск с отображением URL документов
./search_engine.out search index_stemmed.bin ../data/corpus.txt.urls --stemming
```