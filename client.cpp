#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

using namespace std;

#define BUFLEN 1000

bool running = true; // 用來控制執行緒的運行

// 處理傳輸的執行緒函數
void *transaction(void *p)
{
    int *input = (int *)p;
    int sd = input[0];     // 伺服器端 socket 描述符
    int sockfd = input[1]; // 客戶端 socket 描述符

    while (running)
    {
        struct sockaddr_in client;
        int clientfd = 0;
        socklen_t clientlen = sizeof(client);

        // 等待接受新的連線
        clientfd = accept(sockfd, (struct sockaddr *)&client, &clientlen);
        if (clientfd < 0)
        {
            if (!running)
                break; // 如果運行已經被標記為結束，跳出循環
            cerr << "Accept failed" << endl;
            continue;
        }

        char received[BUFLEN] = {};
        // 接收從其他客戶端傳來的訊息
        recv(clientfd, received, sizeof(received), 0);

        // 將訊息轉發至伺服器
        send(sd, received, sizeof(received), 0);

        // 清除接收到的訊息
        bzero(received, sizeof(received));
    }
    close(sockfd);      // 關閉 socket
    pthread_exit(NULL); // 結束執行緒
}

// 顯示使用說明
void printUsage()
{
    cout << "\n使用指令說明:" << endl;
    cout << "1. REGISTER#<username> - 註冊使用者帳號" << endl;
    cout << "2. <username>#<port number> - 登入使用者帳號" << endl;
    cout << "3. List - 列出所有上線的使用者" << endl;
    cout << "4. <轉帳者名稱>#<金額>#<接收者名稱> - 發送金額給另一位使用者 (例如: Cindy#100#John)" << endl;
    cout << "5. Exit - 結束連線" << endl;
    cout << "--------------------------------------------" << endl;
}

int main(int argc, char *argv[])
{
    const char *delimiter = "#"; // 分隔符號
    int myport = 0;              // 儲存分離出的埠號
    int sd = 0;                  // 伺服器 socket 描述符

    // 檢查是否有足夠的參數
    if (argc != 3)
    {
        cerr << "用法: " << argv[0] << " <伺服器 IP> <埠號>" << endl;
        return -1;
    }

    // 建立伺服器 socket
    sd = socket(PF_INET, SOCK_STREAM, 0);
    if (sd < 0)
    {
        cerr << "Failed to create socket" << endl;
        return -1;
    }

    // 設定伺服器位址
    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = PF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = htons(atoi(argv[2]));

    // 嘗試連接伺服器
    int retcode = connect(sd, (struct sockaddr *)&server_address, sizeof(server_address));
    if (retcode < 0)
    {
        cerr << "Connection error!" << endl;
        return -1;
    }
    else
    {
        cout << "已成功連接到伺服器!" << endl;
        printUsage(); // 顯示使用說明
        cout << ">" << flush;
    }

    while (true)
    {
        char message[BUFLEN] = {};
        char message_receive[BUFLEN] = {};
        cin >> message;

        // 發送訊息給伺服器
        send(sd, message, strlen(message), 0);

        // 接收來自伺服器的回應
        recv(sd, message_receive, sizeof(message_receive), 0);
        cout << message_receive << "--------------------" << endl
             << ">";

        // 判斷是否結束連線
        if (strcmp(message, "Exit") == 0)
        {
            cout << "連線已結束" << endl;
            close(sd);
            return 0;
        }
        // 判斷是否要繼續處理 P2P 連線
        else if (strcmp(message_receive, "220 AUTH_FAIL\n") != 0 &&
                 strcmp(message_receive, "210 FAIL\n") != 0 &&
                 strcmp(message_receive, "230 Input format error\n") != 0 &&
                 strcmp(message, "List") != 0 &&
                 strcmp(message_receive, "100 OK\n") != 0)
        {
            // 從訊息中解析出 port
            char *instructor;
            char temp_message[BUFLEN] = {};
            strcpy(temp_message, message);
            instructor = strtok(temp_message, delimiter);
            instructor = strtok(NULL, delimiter);
            myport = atoi(instructor);
            break;
        }
    }

    // 建立客戶端 socket
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        cerr << "Failed to create socket" << endl;
        return -1;
    }

    // 設定客戶端位址
    struct sockaddr_in client_s;
    bzero(&client_s, sizeof(client_s));
    client_s.sin_family = PF_INET;
    client_s.sin_addr.s_addr = inet_addr(argv[1]);
    client_s.sin_port = htons(myport);

    // 綁定 socket
    ::bind(sockfd, (struct sockaddr *)&client_s, sizeof(client_s));

    // 開始監聽連線
    listen(sockfd, 10);

    // 將兩個 socket 描述符打包傳給執行緒
    int two_socket[2] = {sd, sockfd};
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    // 創建執行緒來處理 P2P 傳輸
    if (pthread_create(&tid, &attr, transaction, &two_socket) != 0)
    {
        cerr << "Failed to create thread" << endl;
    }

    // 處理來自使用者的輸入訊息
    while (true)
    {
        char message[BUFLEN] = {};
        char message_receive[BUFLEN] = {};
        cin >> message;

        int count = 0;
        for (int i = 0; message[i] != '\0'; i++)
        {
            if (message[i] == '#')
            {
                count++;
            }
        }

        // P2P 傳輸的邏輯處理
        if (count == 2)
        {
            char *payee_name;
            int payee_port = 0;
            char payee_IP[BUFLEN] = {};
            char temp_message[BUFLEN] = {};
            strcpy(temp_message, message);
            char *p = strtok(temp_message, delimiter);

            for (int i = 0; i < 3; i++)
            {
                if (i == 2)
                {
                    payee_name = p;
                }
                p = strtok(NULL, delimiter);
            }

            // 從伺服器更新用戶列表
            while (true)
            {
                cout << "自動從伺服器更新用戶列表中..." << endl;
                send(sd, "List", sizeof("List"), 0);
                recv(sd, message_receive, sizeof(message_receive), 0);
                cout << message_receive << "--------------------" << endl;

                // 查找目標用戶是否在線
                char *find_payee = strstr(message_receive, payee_name);
                if (find_payee == NULL)
                {
                    cout << "請確保收款人上線" << endl;
                    continue;
                }
                else
                {
                    find_payee = strtok(find_payee, delimiter);
                    find_payee = strtok(NULL, delimiter);
                    strcat(payee_IP, find_payee);
                    find_payee = strtok(NULL, delimiter);
                    payee_port = atoi(find_payee);
                    break;
                }
            }

            // 建立與另一位客戶端的連線
            int transfd = socket(PF_INET, SOCK_STREAM, 0);
            if (transfd < 0)
            {
                cerr << "無法建立連線!" << endl;
                continue;
            }

            struct sockaddr_in trans;
            bzero(&trans, sizeof(trans));
            trans.sin_family = PF_INET;
            trans.sin_addr.s_addr = inet_addr("127.0.0.1");
            trans.sin_port = htons(payee_port);

            cout << payee_IP << ":" << payee_port;

            if (connect(transfd, (struct sockaddr *)&trans, sizeof(trans)) < 0)
            {
                cerr << "無法連線至其他用戶!" << endl
                     << ">";
                continue;
            }
            else
            {
                cout << "已連線到其他用戶!" << endl;
            }

            // 發送 P2P 傳輸的訊息
            send(transfd, message, strlen(message), 0);
            recv(sd, message_receive, sizeof(message_receive), 0);
            cout << message_receive << "--------------------" << endl;

            // 更新用戶列表
            cout << "轉帳後自動更新列表..." << endl;
            send(sd, "List", strlen("List"), 0);
            recv(sd, message_receive, sizeof(message_receive), 0);
            cout << message_receive << "--------------------" << endl
                 << ">";
        }
        else
        {
            // 發送一般訊息至伺服器
            send(sd, message, strlen(message), 0);
            recv(sd, message_receive, sizeof(message_receive), 0);

            // 判斷是否結束連線
            if (strcmp(message, "Exit") == 0)
            {
                running = false; // 設置運行標誌為 false
                break;
            }
            cout << message_receive << "--------------------" << endl
                 << ">";
        }
    }

    cout << "Bye\n連線已關閉" << endl;
    close(sockfd);
    close(sd);

    return 0;
}