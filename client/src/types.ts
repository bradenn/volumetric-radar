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
    pan: number,
    tilt: number,
    temperature: number,
    rssi: number,
    channels: Channel[]
    metadata: {
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
        base: number // 80000Hz
        connected: boolean   // 8Hz
        // chirp: number // 1000Hz (once per ms)
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
