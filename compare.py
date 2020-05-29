import sys
import re

if len(sys.argv)-1 != 2:
  print("python3 ./compare.py test-score-before.log test-score-after.log")
  exit(1)

files = sys.argv[1:]
contents = []

for file in files:
    with open(file, 'r') as f:
        contents.append(f.read().split('\n'))

data = {}

c = contents[0]
for line in c:
    if line.startswith('#'):
        test = re.search("^[\#]{4}\s(.*)\s[\#]{4}$", line).group(1)
        data[test] = {}
    if line.startswith('='):
        curr_input = re.search("^={2}\s(.*)\s={2}$", line).group(1)
        data[test][curr_input] = {}
    if line.startswith('Cost: '):
        cost = float(line[6:])
        data[test][curr_input]['cost'] = cost
    if line.startswith('Max heap usage (bytes): '):
        heap = float(line[len('Max heap usage (bytes): '):])
        data[test][curr_input]['heapUsage'] = heap

c = contents[1]
for line in c:
    if line.startswith('#'):
        test = re.search("^[\#]{4}\s(.*)\s[\#]{4}$", line).group(1)
    if line.startswith('='):
        curr_input = re.search("^={2}\s(.*)\s={2}$", line).group(1)
    if line.startswith('Cost: '):
        cost = float(line[6:])
        old_cost = data[test][curr_input]['cost']
        data[test][curr_input]['cost'] = (old_cost - cost)/old_cost
    if line.startswith('Max heap usage (bytes): '):
        heap = float(line[len('Max heap usage (bytes): '):])
        old_heap = data[test][curr_input]['heapUsage'] 
        if old_heap - heap == 0:
            data[test][curr_input]['heapUsage'] = 0.0
        else:
            data[test][curr_input]['heapUsage'] = (old_heap - heap)/old_heap


with open('improve-report.log', 'w') as f:
    for t in data:
        f.write(f"=={t}==\n")
        perc = {}
        total = 0
        for i in data[t]:
            total += 1
            f.write(f'--{i}--\n')
            for k in data[t][i]:
                temp = perc.get(k, 0.0)
                perc[k] = temp + data[t][i][k]
                f.write(f'{k} improve %: {data[t][i][k]:.5f}%\n')
        f.write('------\n')
        for k in perc:
            f.write(f'Average {k} improve %: {perc[k]/total:.5f}%\n')
        f.write('======\n')