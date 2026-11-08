package main

import (
	"errors"
	"fmt"
	// "io"
	"net"
	"os"
	"os/exec"
	"os/signal"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"
)

const PORT_NUMBER int = 8080

var ln net.Listener
var mutex sync.Mutex

// var channel chan bool
var counter uint64 = 0

const (
	IPv4 = iota
	IPv6
)

// Handles client connections.
func handleConnection(conn net.Conn) {
	// Always close the connection, even after an error
	defer func() {
		err := conn.Close()
		if err != nil {
			fmt.Println("net.Conn.Close:", err)
		}
	}()

	// Reading the request happens here rather than in main() so that a slow
	// or silent client only holds up its own goroutine, not the accept loop.
	// The deadline bounds how long an idle client can tie it up.
	err := conn.SetReadDeadline(time.Now().Add(10 * time.Second))
	if err != nil {
		fmt.Println("net.Conn.SetReadDeadline:", err)
		return
	}
	clientMessageBytes := make([]byte, 4096)
	// use io.ReadFull(conn, client_message_bytes) to wait for clientMessageBytes to fill up entirely
	n, err := conn.Read(clientMessageBytes)
	if err != nil {
		fmt.Println("net.Conn.Read:", err)
		return
	}
	// Only look at the bytes actually received, not the whole zeroed buffer
	clientMessage := string(clientMessageBytes[:n])
	requestLine, _, _ := strings.Cut(clientMessage, "\r\n")
	// requestLine := clientMessage[:strings.Index(clientMessage, "\r\n")]
	// requestLine := clientMessage[:strings.IndexRune(clientMessage, '\r\n')]
	// Only accepting HTTP GET requests
	if !strings.HasPrefix(requestLine, "GET") {
		return
	}
	fmt.Println(requestLine)

	// critical section
	mutex.Lock() // using a mutex
	// <-channel // using a channel
	counter++
	fmt.Printf("Handling request #%v\n", counter)
	// channel <- true
	mutex.Unlock()

	fullAddress := conn.RemoteAddr().String()
	fullAddress = strings.ReplaceAll(fullAddress, "[", "")
	fullAddress = strings.ReplaceAll(fullAddress, "]", "")
	lastIndex := strings.LastIndex(fullAddress, ":")

	serverMessage := "HTTP/1.1 200 OK\r\n"

	date := time.Now().UTC().Format(time.RFC1123)
	serverMessage += "Date: " + strings.Replace(date, "UTC", "GMT\r\n", 1)

	serverMessage += "Server: Web Server\r\n"
	serverMessage += "Last-Modified: Thu, 4 Apr 2024 16:45:18 GMT\r\n"
	serverMessage += "Accept-Ranges: bytes\r\n"

	content := "What's up? This server was written in Go. Your IP address is " + fullAddress[:lastIndex] + "\r\n"

	serverMessage += fmt.Sprintf("Content-Length: %v\r\n", len(content))
	serverMessage += "Content-Type: text/html\r\n\r\n"
	serverMessage += content

	_, err = conn.Write([]byte(serverMessage))
	if err != nil {
		fmt.Println("net.Conn.Write:", err)
	}
}

// Signal handler for Ctrl + C (SIGINT).
func signalHandler() {
	err := ln.Close()
	if err != nil {
		fmt.Println("net.Listener.Close:", err)
	}
	os.Exit(0)
}

// Main server loop for the web server.
func main() {
	var err error

	netType := IPv4
	switch netType {
	case IPv4:
		fmt.Println("Using IPv4")
	case IPv6:
		fmt.Println("Using IPv6")
	default:
		fmt.Println("Unknown protocol.")
	}

	date, err := exec.Command("date").Output()
	if err != nil {
		fmt.Println("exec.Cmd.Output:", err)
	}
	fmt.Printf("Current time: %s", string(date))

	ipaddrs, err := net.LookupHost("www.google.com")
	if err != nil {
		fmt.Println("net.LookupHost:", err)
	}
	fmt.Println("IPv4 addresses associated with www.google.com:")
	for _, res := range ipaddrs {
		if len(res) < 16 {
			fmt.Println(res)
		}
	}

	var portNumber = 0
	if len(os.Args) == 2 {
		portNumber, _ = strconv.Atoi(os.Args[1])
		portNumber &= 65535
	}

	// channel = make(chan bool, 1) // creating a channel to work like a binary semaphore
	// channel <- true
	signalChannel := make(chan os.Signal, 1)

	// No default way of setting SO_REUSEADDR
	if portNumber >= 10000 {
		// "[::1]:" for IPv6
		ln, err = net.Listen("tcp", "127.0.0.1:"+fmt.Sprint(portNumber))
	} else {
		// "[::1]:" for IPv6
		ln, err = net.Listen("tcp", "127.0.0.1:"+fmt.Sprint(PORT_NUMBER))
	}
	if err != nil {
		fmt.Println("net.Listen:", err)
		goto shutdown
	}
	signal.Notify(signalChannel, syscall.SIGINT)
	go func() {
		<-signalChannel
		signalHandler()
	}()

	for {
		// To connect to a server:
		// sock, _ := net.Dial("tcp", "127.0.0.1:5000")
		conn, err := ln.Accept()
		if err != nil {
			if errors.Is(err, net.ErrClosed) {
				goto shutdown
			}
			// e.g. ECONNABORTED or EMFILE: only affects one connection
			fmt.Println("net.Listener.Accept:", err)
			continue
		}
		go handleConnection(conn)
	}

shutdown:
	err = ln.Close()
	if err != nil {
		fmt.Println("net.Listener.Close:", err)
	}
}
