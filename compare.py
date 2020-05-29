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
        print(test)
        data[test] = {}
    if line.startswith('Cost: '):
        cost = float(line[6:])
        data[test]['cost'] = cost
    if line.startswith('Max heap usage (bytes): '):
        heap = float(line[len('Max heap usage (bytes): '):])
        data[test]['heapUsage'] = heap

c = contents[1]
for line in c:
    if line.startswith('#'):
        test = re.search("^[\#]{4}\s(.*)\s[\#]{4}$", line).group(1)
    if line.startswith('Cost: '):
        cost = float(line[6:])
        old_cost = data[test]['cost']
        try:
            data[test]['cost'] = (old_cost - cost)/old_cost
        except:
            data[test]['cost'] = "N/A"
    if line.startswith('Max heap usage (bytes): '):
        heap = float(line[len('Max heap usage (bytes): '):])
        old_heap = data[test]['heapUsage'] 
        try:
            data[test]['heapUsage'] = (old_heap - heap)/old_heap
        except:
            data[test]['heapUsage'] = "N/A"

with open('improve-report.log', 'w') as f:
    for t in data:
       f.write(f"=={t}==\n")
       for k in data[t]:
           f.write(f'{k} improve %: {data[t][k]}\n')