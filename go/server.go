package main

import (
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

	_, err := conn.Write([]byte(serverMessage))
	if err != nil {
		fmt.Println("net.Conn.Write:", err)
		return
	}
	err = conn.Close()
	if err != nil {
		fmt.Println("net.Conn.Close:", err)
		return
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
			fmt.Println("net.Listener.Accept:", err)
			goto shutdown
		}
		clientMessageBytes := make([]byte, 4096)
		// use io.ReadFull(conn, client_message_bytes) to wait for clientMessageBytes to fill up entirely
		_, err = conn.Read(clientMessageBytes)
		if err != nil {
			fmt.Println("net.conn.Read:", err)
			goto shutdown
		}
		clientMessage := string(clientMessageBytes)
		requestLine, _, _ := strings.Cut(clientMessage, "\r\n")
		// requestLine := clientMessage[:strings.Index(clientMessage, "\r\n")]
		// requestLine := clientMessage[:strings.IndexRune(clientMessage, '\r\n')]
		fmt.Println(requestLine)
		go handleConnection(conn)
	}

shutdown:
	err = ln.Close()
	if err != nil {
		fmt.Println("net.Listener.Close:", err)
	}
}
