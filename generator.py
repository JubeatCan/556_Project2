f= open("test1/test2/numbers.txt", "w")
x = 1000000
for i in range(1, x):
    f.write(str(i) + " ")
f.close()
