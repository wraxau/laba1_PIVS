import csv
import subprocess

# Запускать из папки программы после make.
def run(program, threads, search, insert):
    result = subprocess.run(
        ['./' + program, str(threads)],
        input=f'1000 100000 {search} {insert}\n',
        text=True, capture_output=True, check=True
    )
    for line in result.stdout.splitlines():
        if line.startswith('Elapsed time ='):
            return float(line.split()[3])

with open('results.csv', 'w', newline='') as file:
    writer = csv.writer(file)
    writer.writerow(['search', 'insert', 'delete', 'threads', 'custom_s', 'native_s'])
    for search, insert in [(1.0, 0.0), (0.99, 0.005), (0.8, 0.1)]:
        for threads in [1, 2, 4, 8]:
            run('custom', threads, search, insert)  # Прогрев не учитываем.
            run('native', threads, search, insert)
            custom_times = []
            native_times = []
            for repeat in range(5):
                # Чередуем порядок двух версий.
                programs = ['custom', 'native'] if repeat % 2 == 0 else ['native', 'custom']
                for program in programs:
                    elapsed = run(program, threads, search, insert)
                    if program == 'custom':
                        custom_times.append(elapsed)
                    else:
                        native_times.append(elapsed)
            custom = sum(custom_times) / 5
            native = sum(native_times) / 5
            writer.writerow([search, insert, round(1 - search - insert, 3), threads, custom, native])
            print(f'read={search}, threads={threads}: custom={custom:.6f}, native={native:.6f}', flush=True)
