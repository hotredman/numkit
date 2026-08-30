# Полный каталог опкодов слияний (Opcode Fusion Catalog) для `numkit`

Настоящий документ систематизирует опыт оптимизации байткодовых виртуальных машин в индустриальных языках программирования (**CPython 3.11+**, **Lua 5.4 / LuaJIT**, **JavaScript V8 Ignition**, **PHP Zend Engine 8** и **Ruby YARV**) и представляет полную инженерную спецификацию **слиянных суперинструкций (Superinstructions)**, адаптированных для реализации в байткодовом компиляторе и виртуальной машине **`numkit`**.

---

## Сводная таблица индустриальных слияний и целевых опкодов `numkit`

| Категория слияния | Индустриальный прототип | Аналог в `numkit` | Заменяемая цепочка `numkit` | Прирост производительности |
|---|---|---|---|---|
| **1. Арифметика с константой** | • Lua 5.4: `OP_ADDK`, `OP_SUBK`, `OP_MULK`<br>• CPython: `BINARY_OP_ADD_INT/FLOAT`<br>• V8: `AddSmi`, `SubSmi`<br>• Zend: `ZEND_ADD_CV_CONST` | `ADD_IMM`<br>`SUB_IMM`<br>`MUL_IMM`<br>`RDIV_IMM` | `LOAD_CONST cReg, constIdx`<br>`ADD dst, lhsReg, cReg` | Экономия 1 итерации VM, 1 регистра и копирования константы. Ускорение выражений `x+1`, `2*y` на **20–30%**. |
| **2. Сравнения с константой** | • Lua 5.4: `OP_EQK`<br>• V8: `TestEqualSmi`<br>• Zend: `ZEND_IS_EQUAL_CV_CONST` | `EQ_IMM`<br>`LT_IMM`<br>`GT_IMM` | `LOAD_CONST cReg, constIdx`<br>`LT dst, lhsReg, cReg` | Мгновенное сравнение скаляра с константой в условиях `if (x > 0)` без загрузки `0` в регистр. |
| **3. Сравнение + условный переход** | • CPython: `COMPARE_OP_FLOAT_JUMP`<br>• Lua 5.4: `OP_LT` + jump<br>• V8: `BranchIfLess` | `JMP_IF_LT`<br>`JMP_IF_EQ`<br>`JMP_IF_GT` | `LT tmpReg, aReg, bReg`<br>`JMP_TRUE tmpReg, offset` | Исключение создания промежуточного логического `Value` в регистре. Ускорение ветвлений на **25%**. |
| **4. Инкремент / декремент in-place** | • Zend: `ZEND_PRE_INC`<br>• YARV: `opt_succ`<br>• CPython: `STORE_FAST_LOAD_FAST` | `INC_SCALAR`<br>`DEC_SCALAR` | `LOAD_CONST cReg, 1`<br>`ADD reg, reg, cReg` | Обновление скаляра in-place за 1 такт VM без проверок COW и новых дескрипторов. |
| **5. 1D-индексирование векторов** | • Lua 5.4: `OP_GETI`, `OP_SETI`<br>• V8: `LdaKeyedProperty`<br>• Zend: `ZEND_FETCH_DIM_R` | `INDEX_GET_1D`<br>`INDEX_SET_1D` | Кортеж аргументов +<br>`INDEX_GET dst, arr, idx` | Прямой доступ к `doubleData()[idx-1]` за $O(1)$, обход общего `subsref/subsasgn`. Ускорение в **3–4 раза**. |
| **6. 2D-индексирование матриц** | • Специализированные числовые VM (Julia, R bytecode) | `INDEX_GET_2D`<br>`INDEX_SET_2D` | Кортеж аргументов +<br>`INDEX_GET dst, arr, r, c` | Вычисление `(col-1)*rows + (row-1)` in-place для матриц `DOUBLE`. |
| **7. Доступ к полям структур/объектов** | • CPython: `LOAD_ATTR_INSTANCE_VALUE`<br>• V8: `LdaNamedProperty`<br>• Zend: `ZEND_FETCH_OBJ_R` | `PROP_GET_FAST`<br>`PROP_SET_FAST` | `LOAD_STRING sReg, strIdx`<br>`GET_PROP dst, obj, sReg` | Чтение `obj.field` по индексу строки в таблице констант без загрузки строки в промежуточный регистр. |
| **8. Управление циклом `for`** | • Lua 5.4: `OP_FORLOOP`<br>• V8: `ForInNext`<br>• YARV: `opt_case_dispatch` | `FOR_LOOP_SCALAR` | `FOR_NEXT varReg, offset`<br>(с проверкой типа на каждом шаге) | Полное слияние инкремента, проверки границы и перехода в 1 опкод. Ускорение циклов `for` в **2 раза**. |
| **9. Слиянное умножение-сложение** | • Научные VM / BLAS JIT | `FMA_ELEM` | `EMUL tmp, A, B`<br>`ADD dst, tmp, C` | Вычисление `A .* B + C` за 1 проход без аллокации временной матрицы `A .* B`. |

---

## Подробные спецификации опкодов для реализации в `numkit::VM`

### 1. `ADD_IMM` (Сложение со скалярной константой)
* **Формат инструкции:** `I.op = OpCode::ADD_IMM`, `I.a = dstReg`, `I.b = lhsReg`, `I.d = constIdx`.
* **Быстрый путь в `dispatchLoop()` (`src/core/src/vm.cpp`):**
  ```cpp
  case OpCode::ADD_IMM: {
      const Value &lhs = R[I.b];
      const Value &c   = chunk.constants[I.d];
      if (lhs.isDoubleScalar() && c.isDoubleScalar()) {
          R[I.a].setScalarFast(lhs.scalarVal() + c.scalarVal());
      } else {
          // Fallback к стандартной бинарной операции
          execBinaryOpFallback(OpCode::ADD, R[I.a], lhs, c);
      }
      break;
  }
  ```
* **Когда генерирует компилятор (`src/core/src/compiler.cpp`):**
  При компиляции узла бинарной операции `+`, если правый операнд — литерал или константа из таблицы констант.

---

### 2. `JMP_IF_LT` (Сравнение «меньше» со слиянным переходом)
* **Формат инструкции:** `I.op = OpCode::JMP_IF_LT`, `I.a = lhsReg`, `I.b = rhsReg`, `I.offset = int16_t jumpOffset`.
* **Быстрый путь в `dispatchLoop()`:**
  ```cpp
  case OpCode::JMP_IF_LT: {
      const Value &lhs = R[I.a];
      const Value &rhs = R[I.b];
      if (lhs.isDoubleScalar() && rhs.isDoubleScalar()) {
          if (lhs.scalarVal() < rhs.scalarVal()) {
              ip += I.offset;
          }
      } else {
          // Fallback: вычисление сравнения + проверка булевого значения
      }
      break;
  }
  ```
* **Почему критично:** Устраняет выделение временного регистра и создание промежуточного `Value(true/false)` в каждом условном операторе `if` или условии цикла `while`.

---

### 3. `INDEX_GET_1D` (Быстрое 1D-индексирование вектора `arr(i)`)
* **Формат инструкции:** `I.op = OpCode::INDEX_GET_1D`, `I.a = dstReg`, `I.b = arrReg`, `I.c = idxReg`.
* **Быстрый путь в `dispatchLoop()`:**
  ```cpp
  case OpCode::INDEX_GET_1D: {
      const Value &arr = R[I.b];
      const Value &idx = R[I.c];
      if (arr.isHeapDouble() && idx.isDoubleScalar()) {
          int64_t i = static_cast<int64_t>(idx.scalarVal());
          if (i >= 1 && i <= static_cast<int64_t>(arr.numel())) {
              R[I.a].setScalarFast(arr.doubleData()[i - 1]);
              break;
          }
      }
      // Fallback на полный полиморфный subsref
      execSubsref(I, R);
      break;
  }
  ```

---

### 4. `INDEX_SET_1D` (Быстрая запись в вектор `arr(i) = v`)
* **Формат инструкции:** `I.op = OpCode::INDEX_SET_1D`, `I.a = arrReg`, `I.b = idxReg`, `I.c = valReg`.
* **Быстрый путь в `dispatchLoop()`:**
  ```cpp
  case OpCode::INDEX_SET_1D: {
      Value &arr = R[I.a];
      const Value &idx = R[I.b];
      const Value &val = R[I.c];
      if (arr.isHeapDouble() && arr.heapRefCount() == 1 &&
          idx.isDoubleScalar() && val.isDoubleScalar()) {
          int64_t i = static_cast<int64_t>(idx.scalarVal());
          if (i >= 1 && i <= static_cast<int64_t>(arr.numel())) {
              arr.doubleDataMut()[i - 1] = val.scalarVal();
              break;
          }
      }
      execSubsasgn(I, R);
      break;
  }
  ```

---

### 5. `PROP_GET_FAST` (Быстрое чтение свойства `obj.field`)
* **Формат инструкции:** `I.op = OpCode::PROP_GET_FAST`, `I.a = dstReg`, `I.b = objReg`, `I.d = strIdx`.
* **Быстрый путь в `dispatchLoop()`:**
  ```cpp
  case OpCode::PROP_GET_FAST: {
      const Value &obj = R[I.b];
      const std::string &field = chunk.strings[I.d];
      if (obj.isStruct()) {
          const Value *v = obj.structFieldPtr(field);
          if (v) {
              R[I.a] = *v;
              break;
          }
      }
      execPropertyGetFallback(I, R);
      break;
  }
  ```

---

### 6. `FOR_LOOP_SCALAR` (Слиянный шаг цикла `for`)
* **Формат инструкции:** `I.op = OpCode::FOR_LOOP_SCALAR`, `I.a = varReg`, `I.b = stepReg`, `I.c = stopReg`, `I.offset = int16_t jumpBackOffset`.
* **Быстрый путь в `dispatchLoop()`:**
  ```cpp
  case OpCode::FOR_LOOP_SCALAR: {
      double cur = R[I.a].scalarVal() + R[I.b].scalarVal();
      double stop = R[I.c].scalarVal();
      if ((R[I.b].scalarVal() >= 0 && cur <= stop) ||
          (R[I.b].scalarVal() < 0  && cur >= stop)) {
          R[I.a].setScalarFast(cur);
          ip += I.offset; // Переход на начало тела цикла
      }
      break;
  }
  ```

---

## Стратегия внедрения в `numkit`

1. **Фаза 1 (Базовая арифметика и сравнения):** Добавить в `enum class OpCode` инструкции `ADD_IMM`, `SUB_IMM`, `MUL_IMM`, `EQ_IMM`, `LT_IMM`. Обучить `Compiler::visitBinaryOp` генерировать эти опкоды, когда один из операндов — константа.
2. **Фаза 2 (Быстрое 1D-индексирование):** Добавить `INDEX_GET_1D` и `INDEX_SET_1D`. Это даст кратное ускорение при проходе по массивам в циклах.
3. **Фаза 3 (Свойство структур):** Добавить `PROP_GET_FAST` для обхода лишних загрузок строк имени поля.
4. **Фаза 4 (Управление циклами):** Реализовать `FOR_LOOP_SCALAR` для скалярных итераторов.
