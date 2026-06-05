BS8 = chr(8)  # backspace control char that leaked in
t = open("README.md", encoding="utf-8", newline="").read()
bad = "scripts" + BS8 + "uild.bat"
n = t.count(bad)
print("corrupted occurrences:", n)
t = t.replace(bad, "scripts/build.bat")
# safety: also nuke any stray lone backspace chars
stray = t.count(BS8)
t = t.replace(BS8, "")
print("stray backspace chars removed:", stray)
open("README.md", "w", encoding="utf-8", newline="").write(t)
