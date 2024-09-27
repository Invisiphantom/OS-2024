
div = 6
a = (4096 - 24) // div

for i in range(a, -1, -1):
    if i * div + 24 <= 4096 and i % 8 == 0:
        print(i)
        break
    
