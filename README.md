# socket-programming

## 簡介
這是一個兩階段的網路程式設計作業，包含 Client 和 Multi-threaded Server 的開發，主要功能包括使用者註冊、登入、查詢上線名單及金額轉帳。程式開發於 macOS 環境下完成，使用 C++ 編程語言。

---

## 環境
- 作業系統: macOS Sonoma 14.6.1
- 編程語言: C++ (使用 C++11 標準)
- 編譯器: g++
- 工具: Makefile

---

## 第一階段: Client 開發

### 編譯方式
Makefile 指令如下：
```makefile
all: client.cpp
	g++ -std=c++11 client.cpp -o client

clean:
	rm -f client
```
在終端機中輸入以下指令進行編譯：
```bash
make
```
清除生成的執行檔：
```bash
make clean
```

### 執行方式
進入包含執行檔的目錄後，執行以下指令：
```bash
./client <IP Address> <Port Number>
```

### 功能指令
- **註冊帳號**: `REGISTER#<Username>`
- **登入**: `<Username>#<Port Number>`
- **列出上線用戶**: `List`
- **金額轉帳**: `<Payer Name>#<Money Amount>#<Payee Name>`
- **結束連線**: `Exit`

---

## 第二階段: Multi-threaded Server 開發

### 編譯方式
Makefile 指令如下：
```makefile
all: server.cpp
	g++ -std=c++11 server.cpp -o server

clean:
	rm -f server
```
在終端機中輸入以下指令進行編譯：
```bash
make
```
清除生成的執行檔：
```bash
make clean
```

### 執行方式
進入包含執行檔的目錄後，執行以下指令：
```bash
./server <Port Number>
```

---

## GUI 和錯誤處理
- 提供簡單的 GUI，讓使用者操作更加直觀。
- 進行錯誤處理，檢查 socket 是否成功連接、綁定和監聽。

---
