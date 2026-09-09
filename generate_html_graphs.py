import csv
import json
import os

def read_csvs():
    rows = []
    
    # Auto-detect whether we are processing CAAS or Local results
    dir_name = 'caas_results'
    suffix = 'caas'
    if not os.path.exists(dir_name):
        dir_name = 'local_results'
        suffix = 'local'
        
    for f in [f'results_baseline_{suffix}.csv', f'results_task1_{suffix}.csv', f'results_task2_{suffix}.csv']:
        path = os.path.join(dir_name, f)
        if os.path.exists(path):
            with open(path, 'r') as file:
                reader = csv.reader(file)
                for r in reader:
                    if r and r[0].strip() != 'Task':
                        try:
                            rows.append({
                                'task': r[0].strip(),
                                'n': int(r[1]),
                                'p': int(r[2]),
                                't': int(r[3]),
                                't_serial': float(r[4]),
                                't_parallel': float(r[5]),
                                't_total': float(r[6])
                            })
                        except Exception as e:
                            pass # Skip headers or malformed rows
    return rows, dir_name

rows, result_dir = read_csvs()

# Compute speedups
serial_times = {r['n']: r['t_total'] for r in rows if r['task'] == 'Baseline_Serial'}

# Pre-calculate a constant 'f' (parallel fraction) for each task and N to get a smooth theoretical curve
f_map = {}
for r in rows:
    if r['p'] == 1 and r['t'] == 1 and r['t_total'] > 0:
        f_map[(r['task'], r['n'])] = r['t_parallel'] / r['t_total']

for r in rows:
    base_t = serial_times.get(r['n'], None)
    if base_t and r['t_total'] > 0:
        r['empirical_speedup'] = base_t / r['t_total']
    else:
        r['empirical_speedup'] = 0

    # Amdahl theoretical speedup using constant f
    f = f_map.get((r['task'], r['n']), 0.99) # Fallback to 0.99 if p=1,t=1 is missing
    total_cores = r['p'] * r['t']
    if total_cores > 1:
        try:
            r['theoretical_speedup'] = 1 / ((1 - f) + (f / total_cores))
        except ZeroDivisionError:
            r['theoretical_speedup'] = 0
    else:
        r['theoretical_speedup'] = 1

# Generate HTML
html = f"""<!DOCTYPE html>
<html>
<head>
    <title>Lab 2 Graphs</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body {{ font-family: sans-serif; margin: 40px; background-color: #f9f9f9; }}
        .chart-grid {{ display: flex; flex-wrap: wrap; gap: 40px; justify-content: center; }}
        .chart-container {{ width: 800px; padding: 20px; background: white; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }}
        h1 {{ text-align: center; color: #333; }}
        h2 {{ color: #555; text-align: center; font-size: 1.2rem; }}
    </style>
</head>
<body>
    <h1>Lab 2 Benchmark Results</h1>
    <div class="chart-grid">
        <div class="chart-container">
            <h2>1. Run Time vs N (Task 1 vs PThreads vs OpenMP)</h2>
            <canvas id="chart1"></canvas>
        </div>
        <div class="chart-container">
            <h2>2. Empirical Speedup vs N (Task 1 vs PThreads vs OpenMP)</h2>
            <canvas id="chart2"></canvas>
        </div>
        <div class="chart-container">
            <h2 id="title3">3. Empirical Speedup vs Cores (Task 1 vs PThreads vs OpenMP)</h2>
            <canvas id="chart3"></canvas>
        </div>
        <div class="chart-container">
            <h2 id="title4">4. Empirical Speedup vs Threads (Hybrid vs Open MPI)</h2>
            <canvas id="chart4"></canvas>
        </div>
        <div class="chart-container">
            <h2 id="title5">5. Hybrid vs PThreads/OpenMP (Total Thread Matching)</h2>
            <canvas id="chart5"></canvas>
        </div>
        <div class="chart-container">
            <h2 id="title6">6. Task 1 Empirical vs Theoretical Speedup (Amdahl's Law)</h2>
            <canvas id="chart6"></canvas>
        </div>
        <div class="chart-container">
            <h2 id="title7">7. Task 2 Empirical vs Theoretical Speedup</h2>
            <canvas id="chart7"></canvas>
        </div>
    </div>

    <script>
        const rawData = {json.dumps(rows)};
        if(rawData.length === 0) {{
            document.body.innerHTML += "<h2 style='color:red'>No data found. Ensure CSVs are in local_results/ folder.</h2>";
        }}
        
        // Helper to filter data
        const getSeries = (task, filterFn, xKey, yKey) => {{
            let filtered = rawData.filter(d => d.task === task && filterFn(d));
            filtered.sort((a, b) => a[xKey] - b[xKey]);
            return filtered.map(d => ({{ x: d[xKey], y: d[yKey] }}));
        }};

        // Find the maximum N that is shared across all tasks, in case the SLURM job timed out early!
        const tasks = [...new Set(rawData.map(d => d.task))];
        const maxN = Math.max(...rawData.map(d => d.n).filter(n => tasks.every(t => rawData.some(d => d.task === t && d.n === n))));

        // Update titles to explicitly state the N value
        document.getElementById('title3').innerText += ' (Plotted exactly at N = ' + maxN + ')';
        document.getElementById('title4').innerText += ' (Plotted exactly at N = ' + maxN + ')';
        document.getElementById('title5').innerText += ' (Plotted exactly at N = ' + maxN + ')';
        document.getElementById('title6').innerText += ' (Plotted exactly at N = ' + maxN + ')';
        document.getElementById('title7').innerText += ' (Plotted exactly at N = ' + maxN + ')';

        const commonOptions = {{ responsive: true, plugins: {{ legend: {{ position: 'bottom' }} }} }};

        // Chart 1: Time vs N (Procs=4, Threads=4)
        new Chart(document.getElementById('chart1'), {{
            type: 'line',
            data: {{
                datasets: [
                    {{ label: 'Task 1 (Open MPI, p=4)', data: getSeries('Task1', d => d.p === 4, 'n', 't_total'), borderColor: 'red' }},
                    {{ label: 'PThreads (t=4)', data: getSeries('Baseline_PThreads', d => d.t === 4, 'n', 't_total'), borderColor: 'blue' }},
                    {{ label: 'OpenMP (t=4)', data: getSeries('Baseline_OMP', d => d.t === 4, 'n', 't_total'), borderColor: 'green' }}
                ]
            }},
            options: {{ ...commonOptions, scales: {{ x: {{ type: 'linear', title: {{ display: true, text: 'Size of N' }} }}, y: {{ title: {{ display: true, text: 'Total Time (Seconds)' }} }} }} }}
        }});

        // Chart 2: Empirical Speedup vs N
        new Chart(document.getElementById('chart2'), {{
            type: 'line',
            data: {{
                datasets: [
                    {{ label: 'Task 1 (Open MPI, p=4)', data: getSeries('Task1', d => d.p === 4, 'n', 'empirical_speedup'), borderColor: 'red' }},
                    {{ label: 'PThreads (t=4)', data: getSeries('Baseline_PThreads', d => d.t === 4, 'n', 'empirical_speedup'), borderColor: 'blue' }},
                    {{ label: 'OpenMP (t=4)', data: getSeries('Baseline_OMP', d => d.t === 4, 'n', 'empirical_speedup'), borderColor: 'green' }}
                ]
            }},
            options: {{ ...commonOptions, scales: {{ x: {{ type: 'linear', title: {{ display: true, text: 'Size of N' }} }}, y: {{ title: {{ display: true, text: 'Speedup' }} }} }} }}
        }});

        // Chart 3: Speedup vs Cores (Max N)
        new Chart(document.getElementById('chart3'), {{
            type: 'line',
            data: {{
                datasets: [
                    {{ label: 'Task 1 (Open MPI)', data: getSeries('Task1', d => d.n === maxN, 'p', 'empirical_speedup'), borderColor: 'red' }},
                    {{ label: 'PThreads', data: getSeries('Baseline_PThreads', d => d.n === maxN, 't', 'empirical_speedup'), borderColor: 'blue' }},
                    {{ label: 'OpenMP', data: getSeries('Baseline_OMP', d => d.n === maxN, 't', 'empirical_speedup'), borderColor: 'green' }}
                ]
            }},
            options: {{ ...commonOptions, scales: {{ x: {{ type: 'linear', title: {{ display: true, text: 'Number of Processes/Threads' }} }}, y: {{ title: {{ display: true, text: 'Speedup' }} }} }} }}
        }});

        // Chart 4: Hybrid vs Open MPI
        new Chart(document.getElementById('chart4'), {{
            type: 'line',
            data: {{
                datasets: [
                    {{ label: 'Task 1 (Open MPI, scaling p)', data: getSeries('Task1', d => d.n === maxN, 'p', 'empirical_speedup'), borderColor: 'red' }},
                    {{ label: 'Task 2 (Hybrid, p=1, scaling t)', data: getSeries('Task2', d => d.n === maxN && d.p === 1, 't', 'empirical_speedup'), borderColor: 'purple' }}
                ]
            }},
            options: {{ ...commonOptions, scales: {{ x: {{ type: 'linear', title: {{ display: true, text: 'Number of Processes or Threads' }} }}, y: {{ title: {{ display: true, text: 'Speedup' }} }} }} }}
        }});

        // Chart 5: Hybrid vs PThreads/OMP (Total Threads)
        let hybridMap = {{}};
        rawData.filter(d => d.task === 'Task2' && d.n === maxN).forEach(d => {{
            let tot = d.p * d.t;
            if(!hybridMap[tot] || d.empirical_speedup > hybridMap[tot].y) {{
                hybridMap[tot] = {{x: tot, y: d.empirical_speedup}};
            }}
        }});
        // Clip Hybrid data to match the maximum threads tested by Baseline
        const maxBaselineThreads = Math.max(...rawData.filter(d => d.task === 'Baseline_PThreads').map(d => d.t));
        let hybridData = Object.values(hybridMap).filter(d => d.x <= maxBaselineThreads).sort((a,b) => a.x - b.x);

        new Chart(document.getElementById('chart5'), {{
            type: 'line',
            data: {{
                datasets: [
                    {{ label: 'Task 2 (Hybrid, Best Config)', data: hybridData, borderColor: 'purple' }},
                    {{ label: 'PThreads', data: getSeries('Baseline_PThreads', d => d.n === maxN, 't', 'empirical_speedup'), borderColor: 'blue' }},
                    {{ label: 'OpenMP', data: getSeries('Baseline_OMP', d => d.n === maxN, 't', 'empirical_speedup'), borderColor: 'green' }}
                ]
            }},
            options: {{ ...commonOptions, scales: {{ x: {{ type: 'linear', title: {{ display: true, text: 'Total Threads (p * t)' }} }}, y: {{ title: {{ display: true, text: 'Speedup' }} }} }} }}
        }});

        // Chart 6: Task 1 Amdahl's Law
        new Chart(document.getElementById('chart6'), {{
            type: 'line',
            data: {{
                datasets: [
                    {{ label: 'Empirical Speedup', data: getSeries('Task1', d => d.n === maxN, 'p', 'empirical_speedup'), borderColor: 'red' }},
                    {{ label: 'Theoretical Speedup (Amdahl)', data: getSeries('Task1', d => d.n === maxN, 'p', 'theoretical_speedup'), borderColor: 'orange', borderDash: [5, 5] }}
                ]
            }},
            options: {{ ...commonOptions, scales: {{ x: {{ type: 'linear', title: {{ display: true, text: 'Number of MPI Processes (p)' }} }}, y: {{ title: {{ display: true, text: 'Speedup' }} }} }} }}
        }});

        // Chart 7: Task 2 Amdahl's Law (fixed p=2, scaling t)
        new Chart(document.getElementById('chart7'), {{
            type: 'line',
            data: {{
                datasets: [
                    {{ label: 'Empirical Speedup', data: getSeries('Task2', d => d.n === maxN && d.p === 2, 't', 'empirical_speedup'), borderColor: 'purple' }},
                    {{ label: 'Theoretical Speedup (Amdahl)', data: getSeries('Task2', d => d.n === maxN && d.p === 2, 't', 'theoretical_speedup'), borderColor: 'orange', borderDash: [5, 5] }}
                ]
            }},
            options: {{ ...commonOptions, scales: {{ x: {{ type: 'linear', title: {{ display: true, text: 'Number of OpenMP Threads (t)' }} }}, y: {{ title: {{ display: true, text: 'Speedup' }} }} }} }}
        }});
    </script>
</body>
</html>
"""

output_html = os.path.join(result_dir, 'graphs.html')
with open(output_html, 'w') as f:
    f.write(html)
print(f"Graphs generated successfully in {output_html}!")
