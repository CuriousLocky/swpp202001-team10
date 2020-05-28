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
    if line.beginswith('#'):
        test = re.search("^[\#]{4}\s(.*)\s[\#]{4}$").group(0)
        data[test] = {}
    if line.beginswith('Cost: '):
        cost = float(c[6:])
        data[test]['cost'] = cost
    if line.beginswith('Max heap usage (bytes): '):
        heap = float(c[len('Max heap usage (bytes): '):])
        data[test]['heapUsage'] = heap

c = contents[1]
for line in c:
    if line.beginswith('#'):
        test = re.search("^[\#]{4}\s(.*)\s[\#]{4}$").group(0)
    if line.beginswith('Cost: '):
        cost = float(c[6:])
        old_cost = data[test]['cost']
        data[test]['cost'] = (old_cost - cost)/old_cost
    if line.beginswith('Max heap usage (bytes): '):
        heap = float(c[len('Max heap usage (bytes): '):])
        old_heap = data[test]['heapUsage'] 
        data[test]['heapUsage'] = (old_heap - heap)/old_heap

with open('improve-report.log', 'w') as f:
    for test in data:
       f.write(f"=={test}==\n")
       for k in test:
           f.write(f'{k} improve %: {data[test][k]}\n')