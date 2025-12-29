import sys
import json
from pathlib import Path
from collections import defaultdict, Counter
import time

def analyze_stemming(original_file, stemmed_file, output_file):
    print(f"Анализ стемминга: {original_file} -> {stemmed_file}")
    
    start_time = time.time()
    
    stem_groups = defaultdict(set)
    
    word_to_stem = {}
    
    total_tokens = 0
    changed_tokens = 0
    
    try:
        with open(original_file, 'r', encoding='utf-8') as f_orig, \
             open(stemmed_file, 'r', encoding='utf-8') as f_stem:
            
            for line_orig, line_stem in zip(f_orig, f_stem):
                orig = line_orig.strip()
                stem = line_stem.strip()
                
                if not orig:
                    continue
                
                total_tokens += 1
                if orig != stem:
                    changed_tokens += 1
                
                if orig not in word_to_stem:
                    word_to_stem[orig] = stem
                    stem_groups[stem].add(orig)
                    
                if total_tokens % 5000000 == 0:
                    print(f"Обработано {total_tokens // 1000000}M строк...")
                    
    except FileNotFoundError as e:
        print(f"Ошибка: {e}")
        return

    unique_orig = len(word_to_stem)
    unique_stem = len(stem_groups)
    
    print(f"\nВсего токенов: {total_tokens}")
    print(f"Уникальных словоформ: {unique_orig}")
    print(f"Уникальных стемов: {unique_stem}")
    print(f"Коэффициент сжатия: {(1 - unique_stem / unique_orig) * 100:.2f}%")
    print(f"Изменилось токенов: {changed_tokens} ({changed_tokens / total_tokens * 100:.2f}%)")
    
    sorted_groups = sorted(stem_groups.items(), key=lambda x: len(x[1]), reverse=True)
    
    stats = {
        "total_tokens": total_tokens,
        "unique_original": unique_orig,
        "unique_stemmed": unique_stem,
        "compression_ratio": round((1 - unique_stem / unique_orig), 4),
        "changed_tokens_count": changed_tokens,
        "changed_tokens_percent": round(changed_tokens / total_tokens * 100, 2),
        "top_stem_groups": []
    }
    
    print("\nТоп-20 стемов по количеству объединенных словоформ:")
    for i, (stem, forms) in enumerate(sorted_groups[:20], 1):
        forms_list = sorted(list(forms))
        forms_display = ", ".join(forms_list[:10])
        if len(forms_list) > 10:
            forms_display += f" (+ еще {len(forms_list) - 10})"
            
        print(f"{i}. '{stem}' ({len(forms)} форм): {forms_display}")
        
        stats["top_stem_groups"].append({
            "stem": stem,
            "count": len(forms),
            "forms": forms_list[:50]
        })
        
    execution_time = time.time() - start_time
    stats["analysis_time_seconds"] = round(execution_time, 2)
    
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(stats, f, ensure_ascii=False, indent=2)
    
    print(f"\nПодробная статистика сохранена в: {output_file}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 analyze_stemming.py <original_tokens.txt> <stemmed_tokens.txt>")
        print("Example: python3 analyze_stemming.py ../task3.1/cpp/tokens.txt cpp/stemmed_tokens.txt")
        sys.exit(1)
        
    original = sys.argv[1]
    stemmed = sys.argv[2]
    output = "stemming_analysis.json"
    
    analyze_stemming(original, stemmed, output)
