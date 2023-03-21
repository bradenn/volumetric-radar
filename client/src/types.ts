

export interface Device {
    id: string;
    created: string;
    updated: string;
    name: string;
    model: string;
    firmware: string;
    address: string;
    mac: string;
    lastSeen: string;
    connected: boolean;
    frequency: number;
    samples: number;
    hFov: number;
    vFov: number;
}


export interface Zone {
    name: string
    corners: number[][]
}

export interface Unit {
    channels: Channel[]
    metadata: {
        name: string // 80000Hz
        mac: string // 80000Hz
        base: number // 80000Hz
        window: number // 80000Hz
        samples: number   // 8Hz
        prf: number   // 8Hz
        connected: boolean   // 8Hz
        frequency: number   // 10000Hz
        chirp: number // 1000Hz (once per ms)
    }
    rate: number
    phase: number[]
    distance: number[]
}

export interface Channel {
    signalI: number[]
    signalQ: number[],
    spectrum: number[],
    frequencies: number[],
    peaks: number[],
}
