import matplotlib.pyplot as plt
import numpy as np
from collections import Counter
import json
from pathlib import Path
import time


def load_tokens(tokens_file: str) -> list[str]:
    print(f"Загрузка токенов из {tokens_file}...")
    tokens = []
    with open(tokens_file, 'r', encoding='utf-8') as f:
        for line in f:
            token = line.strip()
            if token:
                tokens.append(token)
    return tokens


def calculate_frequencies(tokens: list[str]) -> list[tuple[str, int]]:
    print("Подсчет частот токенов...")
    freq = Counter(tokens)
    sorted_freq = sorted(freq.items(), key=lambda x: x[1], reverse=True)
    return sorted_freq


def build_zipf_plot(sorted_freq: list[tuple[str, int]], output_dir: str):
    print("Построение графика...")
    
    ranks = np.arange(1, len(sorted_freq) + 1)
    frequencies = np.array([f for _, f in sorted_freq])

    fig, ax = plt.subplots(figsize=(12, 8))
    
    ax.loglog(ranks, frequencies, 'b-', linewidth=0.5, alpha=0.7, label='Реальное распределение')
    
    C = frequencies[0]
    alpha = 1.0
    zipf_theoretical = C / (ranks ** alpha)
    
    ax.loglog(ranks, zipf_theoretical, 'r--', linewidth=2, 
              label='Закон Ципфа')

    log_ranks = np.log(ranks)
    log_freqs = np.log(frequencies)
    
    n_points = len(ranks) // 2
    coeffs = np.polyfit(log_ranks[:n_points], log_freqs[:n_points], 1)
    alpha_fit = -coeffs[0]
    C_fit = np.exp(coeffs[1])
    
    ax.set_xlabel('Ранг (log)', fontsize=12)
    ax.set_ylabel('Частота (log)', fontsize=12)
    ax.set_title('Закон Ципфа: распределение терминов по частотностям', fontsize=14)
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3, which='both')
    
    plt.tight_layout()
    
    output_path = Path(output_dir) / 'zipf_plot.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"График сохранен: {output_path}")
    
    return alpha_fit, C_fit


def save_top_tokens(sorted_freq: list[tuple[str, int]], output_dir: str, n: int = 100):
    output_path = Path(output_dir) / 'top_tokens.txt'
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("Ранг\tТокен\tЧастота\n")
        for i, (token, freq) in enumerate(sorted_freq[:n], 1):
            f.write(f"{i}\t{token}\t{freq}\n")
    print(f"Топ-{n} токенов сохранены: {output_path}")


def save_statistics(sorted_freq: list[tuple[str, int]], alpha_fit: float, 
                    C_fit: float, output_dir: str, execution_time: float):
    frequencies = [f for _, f in sorted_freq]
    
    stats = {
        "total_unique_tokens": len(sorted_freq),
        "total_token_occurrences": sum(frequencies),
        "max_frequency": frequencies[0],
        "min_frequency": frequencies[-1],
        "median_frequency": int(np.median(frequencies)),
        "zipf_exponent_fitted": round(alpha_fit, 4),
        "zipf_constant_fitted": round(C_fit, 2),
        "execution_time_seconds": round(execution_time, 2)
    }
    
    output_path = Path(output_dir) / 'zipf_statistics.json'
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(stats, f, ensure_ascii=False, indent=2)
    print(f"Статистика сохранена: {output_path}")
    
    return stats


def analyze_deviations(sorted_freq: list[tuple[str, int]], alpha_fit: float, C_fit: float):
    ranks = np.arange(1, len(sorted_freq) + 1)
    frequencies = np.array([f for _, f in sorted_freq])

    zipf_theoretical = C_fit / (ranks ** alpha_fit)

    relative_error = (frequencies - zipf_theoretical) / zipf_theoretical
    
    print(f"\nПодобранный показатель степени α = {alpha_fit:.4f}")

    tail_start = len(sorted_freq) // 2
    tail_error = np.mean(np.abs(relative_error[tail_start:]))
    head_error = np.mean(np.abs(relative_error[:1000]))
    
    print(f"\nСредняя ошибка для топ-1000 токенов: {head_error*100:.1f}%")
    print(f"Средняя ошибка для хвоста (редкие слова): {tail_error*100:.1f}%")


def main():
    start_time = time.time()
    
    tokens_file = "../tokenizer/tokens.txt"
    output_dir = "./results"
    
    Path(output_dir).mkdir(parents=True, exist_ok=True)
    
    tokens = load_tokens(tokens_file)
    sorted_freq = calculate_frequencies(tokens)
    
    alpha_fit, C_fit = build_zipf_plot(sorted_freq, output_dir)
    
    save_top_tokens(sorted_freq, output_dir)
    
    execution_time = time.time() - start_time
    stats = save_statistics(sorted_freq, alpha_fit, C_fit, output_dir, execution_time)

    analyze_deviations(sorted_freq, alpha_fit, C_fit)
    
    print(f"\nВремя выполнения: {execution_time:.2f} секунд")
    print(f"Всего уникальных токенов: {stats['total_unique_tokens']:,}")
    print(f"Всего вхождений: {stats['total_token_occurrences']:,}")


if __name__ == "__main__":
    main()
