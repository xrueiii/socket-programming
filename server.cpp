#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <queue>
#include <condition_variable>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

using namespace std;

// 使用者資訊的結構體，記錄用戶狀態與帳戶資料
struct UserInfo
{
    bool login = false;  // 用戶是否登入
    string ip;           // 用戶的 IP 地址
    int port;            // 用戶的埠號
    string username;     // 用戶名稱
    int balance = 10000; // 初始餘額，預設為 10000

    // 帶參數的構造函數，方便初始化用戶資訊
    UserInfo(bool login, const string &ip, int port, const string &username, int balance)
        : login(login), ip(ip), port(port), username(username), balance(balance) {}

    // 預設構造函數，允許不帶參數初始化
    UserInfo() = default;
};

// 全域變數，用於管理用戶與伺服器的資料
unordered_map<int, string> clientToUserMap; // 用於記錄連線 Socket 與用戶名的對應關係
mutex clientMapMutex;                       // 保護 clientToUserMap 的鎖

unordered_map<string, UserInfo> userAccounts; // 所有用戶帳戶資料的容器
mutex userMutex;                              // 保護 userAccounts 的鎖

unordered_map<string, UserInfo> online_user_num;

queue<int> taskQueue;       // 任務佇列，存放等待處理的客戶端 Socket
mutex queueMutex;           // 保護 taskQueue 的鎖
condition_variable condVar; // 用於通知執行緒處理任務的條件變數

// 檢查用戶是否已登入
bool isLoggedIn(const string &username)
{
    lock_guard<mutex> lock(userMutex); // 確保資料操作的執行緒安全
    return userAccounts.find(username) != userAccounts.end() && userAccounts[username].login;
}

// 獲取目前在線用戶數
int getOnlineUserCount()
{
    lock_guard<mutex> lock(userMutex); // 確保資料操作的執行緒安全
    int count = 0;
    for (const auto &user : userAccounts)
    {
        if (user.second.login)
        {
            count++;
        }
    }
    return count; // 回傳線上用戶數
}

// 處理每個客戶端的邏輯
void handleClient(int serverSocket, int clientSocket)
{
    char buffer[1024] = {}; // 用於接收客戶端訊息的緩衝區
    while (true)
    {
        memset(buffer, 0, sizeof(buffer));                                // 清空緩衝區，避免殘留數據影響
        int byteReceived = recv(clientSocket, buffer, sizeof(buffer), 0); // 接收客戶端訊息
        string message(buffer);                                           // 將訊息轉為字串格式
        if (byteReceived <= 0)
        {
            // 如果接收數據小於等於 0，表示客戶端斷開或出錯
            cout << "Client disconnected." << endl;
            close(clientSocket); // 關閉該客戶端的 Socket
            break;
        }
        cout << "Get message from client:" << message << endl;

        // 分析收到的訊息，判斷請求的功能
        size_t pos1 = message.find("#");           // 第一次出現 '#' 的位置
        size_t pos2 = message.find("#", pos1 + 1); // 第二次出現 '#' 的位置

        // 處理註冊請求
        if (message.find("REGISTER#") == 0)
        {
            string username = message.substr(9); // 提取用戶名
            lock_guard<mutex> lock(userMutex);   // 確保多執行緒操作安全
            if (userAccounts.find(username) == userAccounts.end())
            {
                // 若用戶名不存在，新增帳戶，初始餘額為 10000
                userAccounts[username] = UserInfo{false, "127.0.0.1", 0, username, 10000};
                send(clientSocket, "100 OK\r\n", 8, 0); // 回傳註冊成功訊息
            }
            else
            {
                send(clientSocket, "210 FAIL\r\n", 10, 0); // 若用戶名已存在，回傳錯誤訊息
            }
        }
        else if (message.find("#") != string::npos && pos2 == string::npos)
        {
            // 處理登入請求
            size_t pos = message.find("#");
            string username = message.substr(0, pos); // 提取用戶名
            int port = stoi(message.substr(pos + 1)); // 提取埠號
            lock_guard<mutex> lock(userMutex);        // 確保資料一致性
            if (port > 65535 || port < 1024)
            {
                // 如果埠號不在合法範圍，回傳錯誤訊息
                string res = "Port number should be within 1024 ~ 65535\r\n";
                send(clientSocket, res.c_str(), res.size(), 0);
                continue;
            }
            if (userAccounts.find(username) != userAccounts.end())
            {
                // 如果用戶名存在，設定為登入狀態並更新 IP 和埠號
                userAccounts[username].login = true;
                userAccounts[username].ip = "127.0.0.1";
                userAccounts[username].port = port;
                clientToUserMap[clientSocket] = username; // 將 Socket 與用戶名綁定

                online_user_num = userAccounts;
                // 準備線上用戶清單
                int balance = userAccounts[username].balance;
                string online_list;
                int onlineCount = 0;
                for (auto &user : userAccounts)
                {
                    if (user.second.login)
                    {
                        online_list += user.first + "#" + user.second.ip + "#" + to_string(user.second.port) + "\r\n";
                        onlineCount++;
                    }
                }
                string response = to_string(balance) + "\npublic key \n" + to_string(onlineCount) + "\n" + online_list;
                send(clientSocket, response.c_str(), response.size(), 0); // 回傳清單與餘額
            }
            else
            {
                send(clientSocket, "220 AUTH_FAIL\r\n", 16, 0); // 若用戶名不存在，回傳登入失敗訊息
            }
        }
        else if (message == "List")
        {
            // 處理請求線上用戶清單
            string username;
            int balance = 0;
            {
                lock_guard<mutex> lock(clientMapMutex); // 確保資料一致性
                if (clientToUserMap.find(clientSocket) != clientToUserMap.end())
                {
                    username = clientToUserMap[clientSocket];
                    balance = userAccounts[username].balance;
                }
                else
                {
                    send(clientSocket, "Please Login First\r\n", 20, 0);
                    continue;
                }
            }
            lock_guard<mutex> lock(userMutex);
            string response = to_string(balance) + "\n<serverPublicKey>\r\n"; // 構造回應訊息
            string online_list;
            int onlineCount = 0;
            for (auto &user : online_user_num)
            {
                if (user.second.login)
                {
                    online_list += user.first + "#" + user.second.ip + "#" + to_string(user.second.port) + "\r\n";
                    onlineCount++;
                }
            }
            response += to_string(onlineCount) + "\n" + online_list;
            send(clientSocket, response.c_str(), response.size(), 0); // 回傳清單
        }
        else if (pos1 != string::npos && pos2 != string::npos)
        {
            // 處理轉帳請求
            cout << "Processing transfer request..." << endl;
            string sender = message.substr(0, pos1); // 發送者名稱
            int amount;
            try
            {
                amount = stoi(message.substr(pos1 + 1, pos2 - pos1 - 1)); // 提取轉帳金額
            }
            catch (...)
            {
                send(clientSocket, "Invalid transfer amount.\r\n", 26, 0); // 金額格式錯誤
                continue;
            }
            string recipient = message.substr(pos2 + 1); // 接收者名稱

            lock_guard<mutex> lock(userMutex); // 確保資料一致性
            if (userAccounts.find(sender) == userAccounts.end() || !userAccounts[sender].login)
            {
                send(clientSocket, "Sender not logged in or not found.\r\n", 37, 0); // 發送者未登入
            }
            else if (userAccounts.find(recipient) == userAccounts.end())
            {
                send(clientSocket, "Recipient not found.\r\n", 23, 0); // 接收者不存在
            }
            else if (userAccounts[sender].balance < amount)
            {
                send(clientSocket, "Insufficient balance.\r\n", 24, 0); // 餘額不足
            }
            else if (amount <= 0)
            {
                send(clientSocket, "Invalid transfer amount.\r\n", 26, 0); // 金額無效
            }
            else
            {
                // 執行轉帳操作
                userAccounts[sender].balance -= amount;
                userAccounts[recipient].balance += amount;

                // 回傳轉帳成功訊息
                string successMessage = "Transfer Successful: " + to_string(amount) +
                                        " from " + sender + " to " + recipient + ".\r\n";
                send(clientSocket, successMessage.c_str(), successMessage.size(), 0);
                cout << sender << " transferred " << amount << " to " << recipient << ".\n";
            }
        }
        else if (message == "Exit")
        {
            // 處理用戶退出
            string username;
            lock_guard<mutex> lock(clientMapMutex);
            if (clientToUserMap.find(clientSocket) != clientToUserMap.end())
            {
                username = clientToUserMap[clientSocket];
            }
            send(clientSocket, "Bye\r\n", 5, 0);
            // userAccounts.erase(username); // 移除用戶帳戶
            close(clientSocket); // 關閉 Socket
        }
        else
        {
            send(clientSocket, "230 Input Format Error\r\n", 24, 0); // 格式錯誤回應
        }
    }

    close(clientSocket); // 確保 Socket 被正確釋放
}

// 工作執行緒函數，處理任務佇列中的客戶端請求
void worker(int serverSocket)
{
    while (true)
    {
        unique_lock<mutex> lock(queueMutex); // 獲取佇列鎖
        condVar.wait(lock, []
                     { return !taskQueue.empty(); }); // 等待任務加入佇列
        int clientSocket = taskQueue.front();         // 獲取佇列中的任務
        taskQueue.pop();                              // 移除該任務
        lock.unlock();                                // 釋放鎖
        handleClient(serverSocket, clientSocket);     // 處理客戶端請求
    }
}

int main(int argc, char *argv[])
{
    // 初始化伺服器 Socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        cerr << "Can't build socket!" << endl;
        return -1;
    }
    int server_port = stoi(argv[1]); // 伺服器埠號由命令行參數傳入

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(server_port);

    if (::bind(serverSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        cerr << "Cannot bind socket!" << endl;
        return -1;
    }

    // 開啟監聽模式
    listen(serverSocket, 5);
    cout << "Server activated! Waiting for connection..." << endl;
    cout << "Max client number: 5" << endl;

    // 建立工作執行緒池
    vector<thread> threadPool;
    for (int i = 0; i < 5; ++i)
    {
        threadPool.emplace_back([serverSocket]()
                                {
                                    worker(serverSocket); // 傳遞 serverSocket 到 worker 函數
                                });
    }

    while (true)
    {
        sockaddr_in clientAddr = {};
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (sockaddr *)&clientAddr, &clientLen); // 接收客戶端連線
        if (clientSocket >= 0)
        {
            // 提取客戶端 IP 和埠號
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
            int clientPort = ntohs(clientAddr.sin_port);

            // 顯示客戶端資訊
            cout << "Client connected!" << endl;
            cout << "IP = " << clientIP << ", Port = " << clientPort << endl;

            // 將任務加入佇列
            lock_guard<mutex> lock(queueMutex);
            taskQueue.push(clientSocket);
            condVar.notify_one(); // 通知執行緒處理新任務
        }
    }

    // 等待所有工作執行緒結束
    for (auto &t : threadPool)
    {
        t.join();
    }

    close(serverSocket); // 關閉伺服器 Socket
    return 0;
}
