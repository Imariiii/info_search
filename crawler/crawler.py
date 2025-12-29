import sys
import yaml
import time
import signal
import hashlib
from urllib.parse import urljoin, urlparse, urlunparse, parse_qs
import re
from datetime import datetime
import pymongo
from pymongo import MongoClient
from pymongo.errors import ConnectionFailure, DuplicateKeyError
import requests
from requests.adapters import HTTPAdapter
try:
    from urllib3.util.retry import Retry
except ImportError:
    from requests.packages.urllib3.util.retry import Retry
import logging
import threading
from concurrent.futures import ThreadPoolExecutor
import queue


logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('./crawler/crawler.log', encoding='utf-8'),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)


class URLNormalizer:
    def __init__(self, config):
        self.config = config.get('url_normalization', {})
    
    def normalize(self, url):
        try:
            parsed = urlparse(url)
            
            
            if self.config.get('remove_query_params', True):
                parsed = parsed._replace(query='')
            
            
            if self.config.get('remove_fragments', True):
                parsed = parsed._replace(fragment='')
            
            
            if self.config.get('lowercase', True):
                parsed = parsed._replace(
                    netloc=parsed.netloc.lower(),
                    path=parsed.path.lower()
                )
            
            
            if self.config.get('remove_trailing_slash', True):
                if parsed.path.endswith('/') and len(parsed.path) > 1:
                    parsed = parsed._replace(path=parsed.path[:-1])
            
            return urlunparse(parsed)
        except Exception as e:
            logger.warning(f"Ошибка нормализации URL {url}: {e}")
            return url


class URLFilter:
    def __init__(self, sources_config):
        self.sources = []
        
        
        if isinstance(sources_config, dict):
            sources_config = [sources_config]
            
        for source_conf in sources_config:
            source = {
                'name': source_conf.get('source_name', 'Unknown'),
                'include': [re.compile(p) for p in source_conf.get('include_patterns', [])],
                'exclude': [re.compile(p) for p in source_conf.get('exclude_patterns', [])]
            }
            self.sources.append(source)
    
    def get_source_for_url(self, url):
        for source in self.sources:
            
            if any(pattern.match(url) for pattern in source['exclude']):
                continue
                
            
            
            
            if source['include']:
                if any(pattern.match(url) for pattern in source['include']):
                    return source['name']
            else:
                
                
                pass
                
        return None


class SearchRobot:
    def __init__(self, config_path):
        with open(config_path, 'r', encoding='utf-8') as f:
            self.config = yaml.safe_load(f)
        
        
        self.url_normalizer = URLNormalizer(self.config)
        self.url_filter = URLFilter(self.config.get('sources', []))
        
        
        self.db_config = self.config.get('db', {})
        self.logic_config = self.config.get('logic', {})
        self.sources_config = self.config.get('sources', [])
        if isinstance(self.sources_config, dict):
            self.sources_config = [self.sources_config]
            
        
        self._connect_db()
        
        
        self._create_indexes()
        
        
        self.url_queue = queue.Queue()
        
        
        self.queued_urls = set()
        self.queue_lock = threading.Lock()
        
        
        self.processed_urls = set()
        self.processed_lock = threading.Lock()
        
        
        self.running = True
        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)
        
        
        self.local_data = threading.local()
        
        
        self._load_state()
        
        
        self._add_seed_urls()
        
        
        self.pages_crawled = 0
        self.pages_lock = threading.Lock()

    def _connect_db(self):
        try:
            connection_string = f"mongodb://{self.db_config.get('host', 'localhost')}:{self.db_config.get('port', 27017)}"
            
            
            if 'username' in self.db_config and 'password' in self.db_config:
                connection_string = f"mongodb://{self.db_config['username']}:{self.db_config['password']}@{self.db_config.get('host', 'localhost')}:{self.db_config.get('port', 27017)}"
                if 'auth_source' in self.db_config:
                    connection_string += f"/?authSource={self.db_config['auth_source']}"
            
            self.client = MongoClient(connection_string)
            self.db = self.client[self.db_config.get('database', 'ir_corpus')]
            self.collection = self.db[self.db_config.get('collection', 'documents')]
            
            
            self.client.admin.command('ping')
            logger.info("Успешное подключение к MongoDB")
        except ConnectionFailure as e:
            logger.error(f"Ошибка подключения к MongoDB: {e}")
            sys.exit(1)
    
    def _create_indexes(self):
        self.collection.create_index([("url", 1)], unique=True)
        self.collection.create_index([("crawled_at", 1)])
        self.collection.create_index([("source", 1)])
    
    def _get_session(self):
        if not hasattr(self.local_data, 'session'):
            session = requests.Session()
            retry_strategy = Retry(
                total=3,
                backoff_factor=1,
                status_forcelist=[429, 500, 502, 503, 504]
            )
            adapter = HTTPAdapter(max_retries=retry_strategy)
            session.mount("http://", adapter)
            session.mount("https://", adapter)
            
            session.headers.update({
                'User-Agent': self.logic_config.get('user_agent', 'SearchRobot/1.0')
            })
            self.local_data.session = session
        return self.local_data.session
    
    def _signal_handler(self, signum, frame):
        logger.info("Получен сигнал остановки. Завершаем работу потоков...")
        self.running = False
    
    def _save_state(self):
        try:
            
            queue_list = []
            try:
                while True:
                    url = self.url_queue.get_nowait()
                    queue_list.append(url)
                    self.url_queue.task_done()
            except queue.Empty:
                pass
            
            if queue_list:
                state_collection = self.db.get_collection('crawler_state')
                state_collection.update_one(
                    {"_id": "queue"},
                    {"$set": {"url_queue": queue_list}},
                    upsert=True
                )
                logger.info(f"Состояние сохранено: {len(queue_list)} URL в очереди")
        except Exception as e:
            logger.error(f"Ошибка при сохранении состояния: {e}")
    
    def _load_state(self):
        logger.info("Загрузка списка обработанных URL...")
        for doc in self.collection.find({}, {"url": 1}):
            self.processed_urls.add(doc['url'])
        
        logger.info(f"Загружено {len(self.processed_urls)} обработанных URL из БД")
        
        
        state_collection = self.db.get_collection('crawler_state')
        state = state_collection.find_one({"_id": "queue"})
        if state and 'url_queue' in state:
            with self.queue_lock:
                for url in state['url_queue']:
                    if url not in self.queued_urls:
                        self.url_queue.put(url)
                        self.queued_urls.add(url)
            logger.info(f"Загружено {self.url_queue.qsize()} URL из очереди")
    
    def _add_seed_urls(self):
        for source in self.sources_config:
            for url in source.get('seed_urls', []):
                normalized = self.url_normalizer.normalize(url)
                
                with self.processed_lock:
                    in_processed = normalized in self.processed_urls
                
                if not in_processed:
                    if self.url_filter.get_source_for_url(normalized):
                        with self.queue_lock:
                            if normalized not in self.queued_urls:
                                self.url_queue.put(normalized)
                                self.queued_urls.add(normalized)
                                logger.info(f"Добавлен начальный URL: {normalized}")
                else:
                    logger.info(f"Начальный URL уже в базе (пропуск seed): {normalized}")

    def _add_urls_for_recheck(self):
        recheck_interval = self.logic_config.get('recheck_interval', 86400)
        current_time = int(time.time())
        
        
        query = {
            "crawled_at": {"$lt": current_time - recheck_interval}
        }
        
        docs_to_recheck = self.collection.find(query, {"url": 1}).limit(1000)
        
        added = 0
        skipped = 0
        with self.queue_lock:
            for doc in docs_to_recheck:
                url = doc['url']
                if self.url_filter.get_source_for_url(url):
                    if url not in self.queued_urls:
                        self.url_queue.put(url)
                        self.queued_urls.add(url)
                        added += 1
                else:
                    
                    
                    skipped += 1
                    self.collection.update_one(
                        {"url": url},
                        {"$set": {"crawled_at": current_time}}
                    )
        
        if added > 0:
            logger.info(f"Добавлено {added} URL для переобкачки")
        if skipped > 0:
            logger.info(f"Пропущено {skipped} URL (не в текущем конфиге, время сдвинуто)")
        
        return added + skipped

    def _extract_links(self, html_content, base_url):
        links = set()
        link_pattern = re.compile(r'<a[^>]+href=["\']([^"\']+)["\']', re.IGNORECASE)
        
        for match in link_pattern.finditer(html_content):
            href = match.group(1)
            absolute_url = urljoin(base_url, href)
            normalized = self.url_normalizer.normalize(absolute_url)
            
            
            if self.url_filter.get_source_for_url(normalized):
                links.add(normalized)
        
        return links
    
    def _check_if_changed(self, url, current_html):
        current_hash = hashlib.md5(current_html.encode('utf-8')).hexdigest()
        doc = self.collection.find_one({"url": url})
        
        if doc is None:
            return True
        
        saved_hash = hashlib.md5(doc.get('html', '').encode('utf-8')).hexdigest()
        return current_hash != saved_hash

    def _should_recheck(self, url):
        
        
        doc = self.collection.find_one({"url": url}, {"crawled_at": 1})
        
        if doc is None:
            return True 
        
        recheck_interval = self.logic_config.get('recheck_interval', 86400)
        last_crawled = doc.get('crawled_at', 0)
        current_time = int(time.time())
        
        return (current_time - last_crawled) >= recheck_interval

    def crawl_worker(self):
        delay = self.logic_config.get('delay_between_pages', 0.5)
        
        while self.running:
            url = None
            try:
                
                try:
                    url = self.url_queue.get(timeout=1.0)
                except queue.Empty:
                    continue

                
                try:
                    source_name = self.url_filter.get_source_for_url(url)
                    if not source_name:
                        logger.info(f"Пропуск (не в конфиге): {url}")
                        continue

                    
                    with self.processed_lock:
                        is_known = url in self.processed_urls
                    
                    if is_known:
                        if not self._should_recheck(url):
                            logger.info(f"Пропуск (еще не время recheck): {url}")
                            continue
                    
                    
                    session = self._get_session()
                    timeout = self.logic_config.get('request_timeout', 30)
                    
                    response = session.get(url, timeout=timeout, allow_redirects=True)
                    
                    if response.status_code == 429:
                        logger.warning(f"Rate limit for {url}. Sleeping...")
                        time.sleep(5)
                        with self.queue_lock:
                            
                            self.url_queue.put(url)
                        continue
                        
                    response.raise_for_status()
                    
                    content_type = response.headers.get('Content-Type', '').lower()
                    if 'text/html' not in content_type:
                        continue
                    
                    html_content = response.text
                    
                    
                    if len(html_content.encode('utf-8')) > 15 * 1024 * 1024:
                        logger.warning(f"Пропускаем {url} - слишком большой документ")
                        continue

                    
                    if not self._check_if_changed(url, html_content):
                        self.collection.update_one(
                            {"url": url},
                            {"$set": {"crawled_at": int(time.time())}}
                        )
                        with self.pages_lock:
                            self.pages_crawled += 1
                            count = self.pages_crawled
                        logger.info(f"[{count}] Обновлена дата (не изменился): {url}")
                    else:
                        
                        document = {
                            "url": url,
                            "html": html_content,
                            "source": source_name,
                            "crawled_at": int(time.time())
                        }
                        self.collection.update_one(
                            {"url": url},
                            {"$set": document},
                            upsert=True
                        )
                        
                        
                        links = self._extract_links(html_content, url)
                        new_links_count = 0
                        with self.queue_lock, self.processed_lock:
                            for link in links:
                                if link not in self.processed_urls and link not in self.queued_urls:
                                    self.url_queue.put(link)
                                    self.queued_urls.add(link)
                                    new_links_count += 1
                        
                        with self.processed_lock:
                            self.processed_urls.add(url)
                            
                        with self.pages_lock:
                            self.pages_crawled += 1
                            count = self.pages_crawled
                            
                        logger.info(f"[{count}] Скачан (изменен/новый): {url} ({source_name}) +{new_links_count} links")
                
                except Exception as e:
                    logger.error(f"Ошибка при обработке {url}: {e}")
                
                finally:
                    
                    if url:
                        with self.queue_lock:
                            if url in self.queued_urls:
                                self.queued_urls.remove(url)
                        self.url_queue.task_done()
                
                if delay > 0:
                    time.sleep(delay)
                    
            except Exception as e:
                logger.error(f"Критическая ошибка в потоке worker: {e}")

    def run(self):
        logger.info("Запуск поискового робота")
        
        max_pages = self.logic_config.get('max_pages', 0)
        num_threads = self.logic_config.get('num_threads', 4)  
        
        logger.info(f"Число потоков: {num_threads}")
        
        with ThreadPoolExecutor(max_workers=num_threads) as executor:
            futures = []
            for _ in range(num_threads):
                futures.append(executor.submit(self.crawl_worker))
            
            
            try:
                while self.running:
                    
                    if max_pages > 0:
                        with self.pages_lock:
                            if self.pages_crawled >= max_pages:
                                logger.info("Достигнут лимит страниц")
                                self.running = False
                                break
                    
                    if self.url_queue.qsize() < num_threads * 2:
                        
                        added = self._add_urls_for_recheck()
                        if added == 0 and self.url_queue.empty():
                             
                             
                             time.sleep(5)
                             if self.url_queue.empty():
                                 logger.info("Очередь пуста и нечего переобкачивать.")
                                 self.running = False
                                 break
                    
                    time.sleep(1)
                    
            except KeyboardInterrupt:
                self.running = False
                
        self._save_state()
        logger.info(f"Робот остановлен. Всего обработано: {self.pages_crawled}")

def main():
    if len(sys.argv) != 2:
        print("Использование: python crawler.py <путь_к_yaml_конфигу>")
        sys.exit(1)
    
    config_path = sys.argv[1]
    
    try:
        robot = SearchRobot(config_path)
        robot.run()
    except KeyboardInterrupt:
        logger.info("Прервано пользователем")
    except Exception as e:
        logger.error(f"Критическая ошибка: {e}", exc_info=True)
        sys.exit(1)

if __name__ == "__main__":
    main()
