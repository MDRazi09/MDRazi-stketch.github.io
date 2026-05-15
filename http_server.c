/**
 * HTTP Server for Super Market Billing System
 * Backend - Written in C
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 8192

// Structure for product
typedef struct Product {
    int id;
    char name[50];
    float price;
    int quantity;
    float total;
} Product;

// Global variables
int serverSocket = -1;
int clientsConnected = 0;
volatile sig_atomic_t serverRunning = 1;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    printf("\nServer shutting down...\n");
    serverRunning = 0;
    if (serverSocket >= 0) close(serverSocket);
}

// Send HTTP response
void sendResponse(int clientSocket, const char* status, const char* html) {
    char header[256];
    snprintf(header, sizeof(header), 
        "HTTP/1.1 %s\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n", 
        status, strlen(html));
    
    send(clientSocket, header, strlen(header), 0);
    send(clientSocket, html, strlen(html), 0);
}

// Parse and process product data from POST request
int processBillingRequest(int clientSocket, char* requestData) {
    Product products[100];
    int n = 0, i;
    float total = 0, grandTotal = 0;
    int discount = 0;
    
    // Parse number of products
    char *token = strtok(requestData, "\r\n");
    while (token != NULL && n < 100) {
        sscanf(token, "PRODUCT:%d,%s,%.2f,%d", 
               &products[n].id, products[n].name, 
               &products[n].price, &products[n].quantity);
        
        products[n].total = products[n].price * products[n].quantity;
        grandTotal += products[n].total;
        n++;
        token = strtok(NULL, "\r\n");
    }
    
    // Calculate total
    if (grandTotal > 1000) {
        float discountAmount = grandTotal * 0.10;
        grandTotal -= discountAmount;
        discount = 1;
    }
    
    // Generate HTML billing page
    char html[BUFFER_SIZE];
    int htmlLen = 0;
    
    htmlLen += snprintf(html + htmlLen, sizeof(html) - htmlLen,
        "<!DOCTYPE html>\n"
        "<html lang='en'>\n"
        "<head>\n"
        "<meta charset='UTF-8'>\n"
        "<title>Super Market Billing System</title>\n"
        "<link rel='stylesheet' href='style.css'>\n"
        "</head>\n"
        "<body>\n"
        "<div class='container'>\n"
        "<h1>🛒 Super Market Billing System</h1>\n"
        "<div class='header-info'>\n"
        "<p>Generated on: " __DATE__ " " __TIME__ "</p>\n"
        "<p>Team: Razi Attar, Sagar Chityal, Pushkar Gajjam, Onkar Patil</p>\n"
        "</div>\n"
        "<div class='bill-section'>\n"
        "<h2>📋 Bill Details</h2>\n"
        "<table class='bill-table'>\n"
        "<thead>\n"
        "<tr>\n"
        "<th>ID</th>\n"
        "<th>Product Name</th>\n"
        "<th>Price/Unit</th>\n"
        "<th>Quantity</th>\n"
        "<th>Subtotal</th>\n"
        "</tr>\n"
        "</thead>\n"
        "<tbody>\n");
    
    // Add product rows
    for (i = 0; i < n; i++) {
        htmlLen += snprintf(html + htmlLen, sizeof(html) - htmlLen,
            "<tr>\n"
            "<td>%d</td>\n"
            "<td>%s</td>\n"
            "<td>₹%.2f</td>\n"
            "<td>%d</td>\n"
            "<td>₹%.2f</td>\n"
            "</tr>\n",
            products[i].id, products[i].name, products[i].price,
            products[i].quantity, products[i].total);
    }
    
    // Add totals section
    htmlLen += snprintf(html + htmlLen, sizeof(html) - htmlLen,
        "</tbody>\n"
        "<tfoot>\n"
        "<tr class='total-row'>\n"
        "<td colspan='4' class='align-right'>GRAND TOTAL:</td>\n"
        "<td>₹%.2f</td>\n"
        "</tr>\n"
        "</tfoot>\n"
        "</table>\n"
        "</div>\n"
        "</div>\n"
        "</body>\n"
        "</html>\n");
    
    sendResponse(clientSocket, "200 OK", html);
    return n;
}

// Handle HTTP GET request
void handleGetRequest(int clientSocket) {
    char html[1024];
    int len = 0;
    
    html[len++] = snprintf(html + len, sizeof(html) - len,
        "<!DOCTYPE html>\n"
        "<html lang='en'>\n"
        "<head>\n"
        "<meta charset='UTF-8'>\n"
        "<title>Super Market Billing System - Home</title>\n"
        "<link rel='stylesheet' href='style.css'>\n"
        "</head>\n"
        "<body>\n"
        "<div class='container'>\n"
        "<div class='header'>\n"
        "<h1>🛒 Super Market Billing System</h1>\n"
        "<p class='subtitle'>Developed using C Backend & HTML Frontend</p>\n"
        "</div>\n"
        "<div class='main-content'>\n"
        "<div class='welcome-box'>\n"
        "<h2>👋 Welcome!</h2>\n"
        "<p>Add products below and generate billing receipt instantly.</p>\n"
        "</div>\n"
        "<form id='billingForm' class='billing-form'>\n"
        "<div class='form-group'>\n"
        "<label>Product ID:</label>\n"
        "<input type='number' name='id' value='1' min='1' required>\n"
        "</div>\n"
        "<div class='form-group'>\n"
        "<label>Product Name:</label>\n"
        "<input type='text' name='name' placeholder='e.g., Apple' required>\n"
        "</div>\n"
        "<div class='form-group'>\n"
        "<label>Price (₹):</label>\n"
        "<input type='number' name='price' step='0.01' min='0' required>\n"
        "</div>\n"
        "<div class='form-group'>\n"
        "<label>Quantity:</label>\n"
        "<input type='number' name='quantity' value='1' min='1' required>\n"
        "</div>\n"
        "<button type='submit' class='btn-add'>+ Add Product</button>\n"
        "</form>\n"
        "<div id='productsList' class='products-list'></div>\n"
        "<div class='actions'>\n"
        "<button id='generateBillBtn' class='btn-primary' disabled>📄 Generate Bill</button>\n"
        "<button id='clearAllBtn' class='btn-secondary'>🗑️ Clear All</button>\n"
        "</div>\n"
        "</div>\n"
        "</div>\n"
        "<script src='script.js'></script>\n"
        "</body>\n"
        "</html>");
    
    sendResponse(clientSocket, "200 OK", html);
}

// Main server function
int main(int argc, char* argv[]) {
    struct sockaddr_in serverAddr, clientAddr;
    int clientSocket;
    char request[BUFFER_SIZE];
    int clientLen;
    int numProducts = 0;
    
    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    // Allow address reuse
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Configure server address
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);
    
    // Bind and listen
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        return 1;
    }
    
    if (listen(serverSocket, 5) < 0) {
        perror("Listen failed");
        return 1;
    }
    
    printf("\n========================================\n");
    printf("  Super Market Billing Server\n");
    printf("  Server running on http://localhost:%d\n", PORT);
    printf("  Press Ctrl+C to stop\n");
    printf("========================================\n");
    printf("\n");
    
    // Server loop
    while (serverRunning) {
        // Accept client connection
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, NULL);
        if (clientSocket < 0) {
            perror("Accept failed");
            continue;
        }
        
        printf("✓ New client connected\n");
        clientsConnected++;
        
        // Read request
        memset(request, 0, sizeof(request));
        clientLen = recv(clientSocket, request, sizeof(request) - 1, 0);
        if (clientLen <= 0) {
            close(clientSocket);
            continue;
        }
        
        // Parse Content-Type and Content-Length
        char *contentType = NULL;
        char *contentLength = NULL;
        int parsePos = 0;
        
        // Simple parsing for HTTP/POST
        if (strstr(request, "POST")) {
            // Extract data from request
            char *postData = NULL;
            for (parsePos = 0; parsePos < clientLen; parsePos++) {
                if (strncmp(request + parsePos, "Content-Length:", 15) == 0) {
                    char *end = strchr(request + parsePos, '\r');
                    contentLength = end + 2;
                    parsePos = end + 2;
                    break;
                }
            }
            
            if (contentLength && strlen(contentLength) > 0) {
                int dataLen = atoi(contentLength);
                if (dataLen > 0) {
                    postData = malloc(dataLen);
                    for (int i = 0; i < dataLen; i++) {
                        if (recv(clientSocket, postData + i, 1, 0) > 0) break;
                    }
                    postData[dataLen] = '\0';
                    
                    // Process billing data
                    numProducts = processBillingRequest(clientSocket, postData);
                    if (postData) free(postData);
                }
            }
        }
        
        close(clientSocket);
        if (clientsConnected > 0) clientsConnected--;
    }
    
    close(serverSocket);
    printf("\nServer stopped. Goodbye!\n");
    return 0;
}
