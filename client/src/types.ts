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

export interface Metadata {
    chirp: {
        prf: number
        duration: number
        steps: number
        padding: number
        resolution: number
    },
    sampling: {
        frequency: number
        samples: number
        attenuation: number
    },
    name: string // 80000Hz
    mac: string // 80000Hz
    enable: number // 80000Hz
    audible: number // 80000Hz
    gyro: number // 80000Hz
    base: number // 80000Hz
    rate: number // 80000Hz
    duration: number // 80000Hz
    connected: boolean   // 8Hz
    // chirp: number // 1000Hz (once per ms)
}

export interface Unit {
    pan: number,
    tilt: number,
    temperature: number,
    rssi: number,
    channels: Channel[]
    metadata: Metadata
    rate: number
    samples: number
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
