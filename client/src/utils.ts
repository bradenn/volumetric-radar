export function map_range(value: number, low1: number, high1: number, low2: number, high2: number) {
    return low2 + (high2 - low2) * (value - low1) / (high1 - low1);
}

export function lerp(a: number, b: number, t: number): number {
    return (1 - t) * a + t * b;
}

export function findLocalMaximas(arr: number[], threshold: number): number[] {
    const maximas: number[] = [];
    for (let i = 1; i < arr.length - 1; i++) {
        if (arr[i] > arr[i - 1] && arr[i] > arr[i + 1] && arr[i] > threshold) {
            maximas.push(i);
        }
    }
    return maximas;
}

export function formatFrequency(frequency: number): string {
    let unit = "Hz";
    if (frequency >= 1000) {
        frequency /= 1000;
        unit = "kHz";
    }
    if (frequency >= 1000) {
        frequency /= 1000;
        unit = "MHz";
    }
    if (frequency >= 1000) {
        frequency /= 1000;
        unit = "GHz";
    }
    return frequency.toFixed(2) + " " + unit;
}


export function matchedFilter(signal: number[], filter: number[]): number[] {
    const signalLength = signal.length;
    const filterLength = filter.length;
    const resultLength = 2 * (signalLength + filterLength - 1);

    const paddedSignal = new Array(resultLength).fill(0);
    const paddedFilter = new Array(resultLength).fill(0);

    for (let i = 0; i < signalLength; i++) {
        paddedSignal[i + filterLength - 1] = signal[i];
    }

    for (let i = 0; i < filterLength; i++) {
        paddedFilter[i + filterLength - 1] = filter[i];
    }

    const result = new Array(resultLength).fill(0);
    for (let i = 0; i < resultLength; i++) {
        for (let j = 0; j <= i; j++) {
            result[i] += paddedSignal[j] * paddedFilter[i - j];
        }
    }

    return result.slice(filterLength - 1, filterLength - 1 + signalLength);
}

export function cubicSplineInterpolation(points: number[][], resolution: number): number[][] {
    const n = points.length;
    const x = points.map(p => p[0]);
    const y = points.map(p => p[1]);
    const h = [];

    for (let i = 0; i < n - 1; i++) {
        h.push(x[i + 1] - x[i]);
    }

    const alpha = [];
    for (let i = 1; i < n - 1; i++) {
        alpha.push((3 / h[i]) * (y[i + 1] - y[i]) - (3 / h[i - 1]) * (y[i] - y[i - 1]));
    }

    const l = [1];
    const mu = [0];
    const z = [0];
    for (let i = 1; i < n - 1; i++) {
        l.push(2 * (x[i + 1] - x[i - 1]) - h[i - 1] * mu[i - 1]);
        mu.push(h[i] / l[i]);
        z.push((alpha[i - 1] - h[i - 1] * z[i - 1]) / l[i]);
    }

    l.push(1);
    z.push(0);

    const b = [];
    const c = [0];
    const d = [];
    for (let i = n - 2; i >= 0; i--) {
        c.unshift(z[i] - mu[i] * c[0]);
        b.unshift((y[i + 1] - y[i]) / h[i] - h[i] * (c[0] + 2 * c[1]) / 3);
        d.unshift((c[0] - c[1]) / (3 * h[i]));
    }

    const result = [];
    for (let i = 0; i < n - 1; i++) {
        const step = h[i] / resolution;
        for (let j = 0; j < resolution; j++) {
            const t = j * step;
            const x0 = x[i] + t;
            const y0 = y[i] + b[i] * t + c[i] * t ** 2 + d[i] * t ** 3;
            result.push([x0, y0]);
        }
    }

    result.push([x[n - 1], y[n - 1]]);
    return result;
}

export function resampleData(data: number[], targetWidth: number): number[] {
    const currentWidth = data.length;

    // If the target width is smaller than the current width, downsample the data
    if (targetWidth < currentWidth) {
        const factor = currentWidth / targetWidth;
        const downsampled = [];

        for (let i = 0; i < targetWidth; i++) {
            const start = Math.floor(i * factor);
            const end = Math.floor((i + 1) * factor);
            const sum = data.slice(start, end).reduce((acc, d) => acc + d, 0);
            downsampled.push(sum / (end - start));
        }

        return downsampled;
    }

    // If the target width is larger than the current width, interpolate the data
    if (targetWidth > currentWidth) {
        const points = data.map((d, i) => [i, d]);
        const interpolated = cubicSplineInterpolation(points, targetWidth / currentWidth);
        return interpolated.map(p => p[1]);
    }

    // If the target width is the same as the current width, return the original data
    return data;
}