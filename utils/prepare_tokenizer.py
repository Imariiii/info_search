import sys
import time
import json
import yaml
import os
import re
from pymongo import MongoClient
from pymongo.errors import ConnectionFailure
import logging

try:
    from lxml import html as lxml_html
    from lxml.etree import ParserError
    USE_LXML = True
except ImportError:
    from bs4 import BeautifulSoup
    USE_LXML = False
    ParserError = Exception

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

WHITESPACE_RE = re.compile(r'\s+')
HTML_TAG_RE = re.compile(r'<[^>]+>')

BUFFER_SIZE = 100
LOG_INTERVAL = 5000


class TextExtractor:
    """Класс для извлечения текста из HTML (оптимизированный)"""

    def __init__(self):
        self.use_lxml = USE_LXML
        if self.use_lxml:
            logger.info("Используется lxml для парсинга HTML (быстрый режим)")
        else:
            logger.info("Используется BeautifulSoup для парсинга HTML (fallback)")

    def extract_text(self, html_content):
        """
        Извлекает текст из HTML-контента

        Args:
            html_content: HTML-код страницы

        Returns:
            Извлеченный текст
        """
        if not html_content:
            return ""

        try:
            if self.use_lxml:
                return self._extract_with_lxml(html_content)
            else:
                return self._extract_with_beautifulsoup(html_content)
        except Exception as e:
            text = HTML_TAG_RE.sub(' ', html_content)
            text = WHITESPACE_RE.sub(' ', text)
            return text.strip()

    def _extract_with_lxml(self, html_content):
        """Быстрое извлечение текста через lxml"""
        try:
            tree = lxml_html.fromstring(html_content)
        except (ParserError, ValueError):
            try:
                tree = lxml_html.fromstring(f"<html><body>{html_content}</body></html>")
            except:
                return ""

        for element in tree.xpath('//script | //style | //nav | //header | //footer | //aside | //noscript'):
            parent = element.getparent()
            if parent is not None:
                parent.remove(element)

        content_selectors = [
            '//div[@id="mw-content-text"]',
            '//div[contains(@class, "mw-parser-output")]',
            '//main',
            '//article',
            '//body'
        ]

        text = ""
        for selector in content_selectors:
            content = tree.xpath(selector)
            if content:
                text = content[0].text_content()
                break

        if not text:
            text = tree.text_content()

        text = WHITESPACE_RE.sub(' ', text)
        return text.strip()

    def _extract_with_beautifulsoup(self, html_content):
        """Fallback извлечение через BeautifulSoup"""
        soup = BeautifulSoup(html_content, 'html.parser')

        for element in soup.find_all(['script', 'style', 'nav', 'header',
                                      'footer', 'aside', 'noscript']):
            element.decompose()

        content_div = soup.find('div', {'id': 'mw-content-text'}) or \
                      soup.find('div', {'class': 'mw-parser-output'}) or \
                      soup.find('main') or \
                      soup.find('article') or \
                      soup.find('body')

        if content_div:
            text = content_div.get_text(separator=' ', strip=True)
        else:
            body = soup.find('body')
            if body:
                text = body.get_text(separator=' ', strip=True)
            else:
                text = soup.get_text(separator=' ', strip=True)

        text = WHITESPACE_RE.sub(' ', text)
        return text.strip()


class TextStats:
    """Класс для сбора статистики по извлеченным текстам"""

    def __init__(self):
        self.doc_count = 0
        self.total_text_size = 0
        self.total_text_length = 0

    def add_text(self, text):
        """
        Добавляет текст в статистику

        Args:
            text: Текст документа
        """
        self.doc_count += 1
        text_size = len(text.encode('utf-8'))
        self.total_text_size += text_size
        self.total_text_length += len(text)

    def get_stats(self):
        """
        Возвращает статистику

        Returns:
            Словарь со статистикой
        """
        return {
            'total_documents': self.doc_count,
            'total_text_size_bytes': self.total_text_size,
            'total_text_size_kb': self.total_text_size / 1024,
            'total_text_size_mb': self.total_text_size / (1024 * 1024),
            'total_text_length_chars': self.total_text_length,
            'average_text_size_bytes': self.total_text_size / self.doc_count if self.doc_count > 0 else 0,
            'average_text_length_chars': self.total_text_length / self.doc_count if self.doc_count > 0 else 0
        }


def load_db_config(config_path):
    """Загружает конфигурацию БД из YAML"""
    with open(config_path, 'r', encoding='utf-8') as f:
        config = yaml.safe_load(f)
    return config.get('db', {})


def get_mongo_collection(db_config):
    """Подключается к MongoDB и возвращает коллекцию"""
    try:
        connection_string = f"mongodb://{db_config.get('host', 'localhost')}:{db_config.get('port', 27017)}"

        if 'username' in db_config and 'password' in db_config:
            connection_string = f"mongodb://{db_config['username']}:{db_config['password']}@{db_config.get('host', 'localhost')}:{db_config.get('port', 27017)}"
            if 'auth_source' in db_config:
                connection_string += f"/?authSource={db_config['auth_source']}"

        client = MongoClient(connection_string)
        db = client[db_config.get('database', 'ir_corpus')]
        collection = db[db_config.get('collection', 'documents')]

        client.admin.command('ping')
        logger.info("Успешное подключение к MongoDB")

        return collection, client
    except ConnectionFailure as e:
        logger.error(f"Ошибка подключения к MongoDB: {e}")
        sys.exit(1)


def main():
    """Главная функция - извлекает текст из документов и сохраняет в файл"""
    if len(sys.argv) < 2:
        print("Использование: python extract_text.py <путь_к_yaml_конфигу> [output_text_file]")
        print("Пример: python extract_text.py ../configs/crawler_config.yaml corpus.txt")
        sys.exit(1)

    config_path = sys.argv[1]
    output_text_file = sys.argv[2] if len(sys.argv) > 2 else './corpus.txt'

    start_time = time.time()

    logger.info("Загрузка конфигурации...")
    db_config = load_db_config(config_path)

    logger.info("Подключение к MongoDB...")
    collection, client = get_mongo_collection(db_config)

    query_filter = {"html": {"$exists": True, "$ne": "", "$ne": None}}
    total_docs = collection.count_documents(query_filter)
    logger.info(f"Найдено {total_docs} документов с HTML-контентом в MongoDB")

    if total_docs == 0:
        logger.error("Не найдено документов с HTML в MongoDB")
        client.close()
        sys.exit(1)

    extractor = TextExtractor()
    stats = TextStats()

    logger.info("Извлечение текста из документов (потоковая обработка)...")
    texts_written = 0

    batch_size = 500
    
    output_urls_file = output_text_file + ".urls"

    with open(output_text_file, 'w', encoding='utf-8') as f, \
         open(output_urls_file, 'w', encoding='utf-8') as f_urls:
         
        cursor = collection.find(
            query_filter,
            {"html": 1, "url": 1, "_id": 0},
            batch_size=batch_size,
            no_cursor_timeout=True
        )

        text_buffer = []
        url_buffer = []

        try:
            for i, doc in enumerate(cursor, 1):
                if i % LOG_INTERVAL == 0:
                    elapsed = time.time() - start_time
                    speed = i / elapsed if elapsed > 0 else 0
                    logger.info(f"Обработано {i}/{total_docs} ({i*100/total_docs:.1f}%) | "
                               f"Скорость: {speed:.0f} док/сек")

                html_content = doc.get('html', '')
                url_content = doc.get('url', 'N/A')

                text = extractor.extract_text(html_content)

                if not text or len(text.strip()) == 0:
                    continue

                text_buffer.append(text)
                url_buffer.append(url_content)

                stats.add_text(text)
                texts_written += 1

                if len(text_buffer) >= BUFFER_SIZE:
                    f.write('\n\n'.join(text_buffer))
                    f.write('\n\n')
                    
                    f_urls.write('\n'.join(url_buffer))
                    f_urls.write('\n')
                    
                    text_buffer.clear()
                    url_buffer.clear()

            if text_buffer:
                f.write('\n\n'.join(text_buffer))
                f.write('\n\n')
                
                f_urls.write('\n'.join(url_buffer))
                f_urls.write('\n')

        finally:
            cursor.close()
            client.close()

    elapsed_time = time.time() - start_time

    statistics = stats.get_stats()
    statistics['execution_time_seconds'] = elapsed_time
    statistics['execution_time_minutes'] = elapsed_time / 60
    statistics['documents_written'] = texts_written
    statistics['parser_used'] = 'lxml' if USE_LXML else 'BeautifulSoup'

    if statistics['total_text_size_kb'] > 0:
        statistics['kb_per_second'] = statistics['total_text_size_kb'] / elapsed_time
    else:
        statistics['kb_per_second'] = 0

    statistics['docs_per_second'] = texts_written / elapsed_time if elapsed_time > 0 else 0

    meta_file = output_text_file + '.meta.json'
    with open(meta_file, 'w', encoding='utf-8') as f:
        json.dump(statistics, f, ensure_ascii=False, indent=2)

    print("\n" + "="*60)
    print("СТАТИСТИКА ИЗВЛЕЧЕНИЯ ТЕКСТА")
    print("="*60)
    print(f"Парсер: {statistics['parser_used']}")
    print(f"Количество документов: {statistics['total_documents']}")
    print(f"Документов с текстом: {statistics['documents_written']}")
    print(f"\nРазмер текста:")
    print(f"  Всего: {statistics['total_text_size_mb']:.2f} MB ({statistics['total_text_size_kb']:.2f} KB, {statistics['total_text_size_bytes']:,} байт)")
    print(f"  Средний размер документа: {statistics['average_text_size_bytes']:.0f} байт")
    print(f"  Всего символов: {statistics['total_text_length_chars']:,}")
    print(f"  Средняя длина документа: {statistics['average_text_length_chars']:.0f} символов")
    print(f"\nВремя выполнения:")
    print(f"  {statistics['execution_time_seconds']:.2f} секунд ({statistics['execution_time_minutes']:.2f} минут)")
    print(f"\nСкорость обработки:")
    print(f"  {statistics['kb_per_second']:.2f} KB/сек")
    print(f"  {statistics['docs_per_second']:.1f} документов/сек")
    print("="*60)
    print(f"\nТекст сохранен в: {output_text_file}")
    print(f"Ссылки сохранены в: {output_urls_file}")
    print(f"Метаинформация сохранена в: {meta_file}")


if __name__ == "__main__":
    main()
