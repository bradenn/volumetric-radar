import {reactive} from "vue";

interface Bridge {
    hostname: string
    connected: boolean
    socket: WebSocket
}

function getBridgeHost(): string {
    return "ws://localhost:4567/ws"
}

export function useBridge(): Bridge {
    const state = reactive<Bridge>({
        hostname: getBridgeHost(),
        connected: false,
        socket: {} as WebSocket,
    })

    function connect() {
        let socket = new WebSocket(state.hostname)
        socket.onerror = onError
        socket.onopen = onConnect
        socket.onmessage = onMessage
        socket.onclose = onClose
        state.socket = socket
    }

    function onConnect(e: Event) {
        state.connected = true
    }

    function onClose(e: Event) {
        state.connected = false
    }

    function onError(e: Event) {
        state.connected = true
    }

    function onMessage(e: MessageEvent) {
        state.connected = true
    }

    return state
}