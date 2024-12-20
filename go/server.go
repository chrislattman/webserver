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

	full_address := conn.RemoteAddr().String()
	full_address = strings.ReplaceAll(full_address, "[", "")
	full_address = strings.ReplaceAll(full_address, "]", "")
	last_index := strings.LastIndex(full_address, ":")

	server_message := "HTTP/1.1 200 OK\n"
	date := time.Now().UTC().Format(time.RFC1123)
	server_message += "Date: " + strings.Replace(date, "UTC", "GMT\n", 1)
	server_message += "Server: Web Server\n"
	server_message += "Last-Modified: Thu, 4 Apr 2024 16:45:18 GMT\n"
	server_message += "Accept-Ranges: bytes\n"

	content := "What's up? This server was written in Go. Your IP address is " + full_address[:last_index] + "\n"

	server_message += "Content-Length: " + fmt.Sprintln(len(content))
	server_message += "Content-Type: text/html\n\n"
	server_message += content

	_, err := conn.Write([]byte(server_message))
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

	var port_number = 0
	if len(os.Args) == 2 {
		port_number, _ = strconv.Atoi(os.Args[1])
		port_number &= 65535
	}

	// channel = make(chan bool, 1) // creating a channel to work like a binary semaphore
	// channel <- true
	signal_channel := make(chan os.Signal, 1)

	// No default way of setting SO_REUSEADDR
	if port_number >= 10000 {
		// "[::1]:" for IPv6
		ln, err = net.Listen("tcp", "127.0.0.1:"+fmt.Sprint(port_number))
	} else {
		// "[::1]:" for IPv6
		ln, err = net.Listen("tcp", "127.0.0.1:"+fmt.Sprint(PORT_NUMBER))
	}
	if err != nil {
		fmt.Println("net.Listen:", err)
		goto shutdown
	}
	signal.Notify(signal_channel, syscall.SIGINT)
	go func() {
		<-signal_channel
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
		client_message_bytes := make([]byte, 4096)
		// use io.ReadFull(conn, client_message_bytes) to wait for client_message_bytes to fill up entirely
		_, err = conn.Read(client_message_bytes)
		if err != nil {
			fmt.Println("net.conn.Read:", err)
			goto shutdown
		}
		client_message := string(client_message_bytes)
		request_line, _, _ := strings.Cut(client_message, "\n")
		// request_line := clientMessage[:strings.Index(clientMessage, "\n")]
		// request_line := clientMessage[:strings.IndexRune(clientMessage, '\n')]
		fmt.Println(request_line)
		go handleConnection(conn)
	}

shutdown:
	err = ln.Close()
	if err != nil {
		fmt.Println("net.Listener.Close:", err)
	}
}
