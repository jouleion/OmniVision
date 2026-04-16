import pandas as pd

df = pd.read_csv("results.csv")

top_s1 = df.nsmallest(10, "S1_avg_error")
top_s2 = df.nsmallest(10, "S2_avg_error")

print("Top 10 S1:")
print(top_s1)

print("\nTop 10 S2:")
print(top_s2)
