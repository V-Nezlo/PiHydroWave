import { CommonModule, isPlatformBrowser } from '@angular/common';
import {
  AfterViewInit,
  Component,
  ElementRef,
  Inject,
  NgZone,
  OnDestroy,
  OnInit,
  PLATFORM_ID,
  ViewChild,
  signal
} from '@angular/core';

type StatusLevel = 0 | 1 | 2 | 3;

type ValueKind = 'bool' | 'number';

type SettingsSection = 'config' | 'int';
type SettingsValueType = 'bool' | 'number' | 'list' | 'select' | 'time';

interface WidgetState {
  id: string;
  name: string;
  keyBase: string;
  valueKind: ValueKind;
  unit?: string;
  rawValue: unknown;
  status: StatusLevel;
  statusStr: string;
}

interface SystemState {
  mode: string;
  nextSwitchTime: string;
  plainType: string;
  swingState: string;
}

interface SettingsEntry {
  key: string;
  label: string;
  section: SettingsSection;
  type: SettingsValueType;
  unit?: string;
  options?: { value: number; label: string }[];
}

interface SettingsDevice {
  id: string;
  name: string;
  entries: SettingsEntry[];
}

@Component({
  selector: 'app-root',
  imports: [CommonModule],
  templateUrl: './app.html',
  styleUrl: './app.css'
})
export class App implements OnInit, AfterViewInit, OnDestroy {
  readonly widgets = signal<WidgetState[]>([
    {
      id: 'pump',
      name: 'Насос',
      keyBase: 'pump',
      valueKind: 'bool',
      rawValue: null,
      status: 0,
      statusStr: 'Нет данных'
    },
    {
      id: 'lamp',
      name: 'Лампа',
      keyBase: 'lamp',
      valueKind: 'bool',
      rawValue: null,
      status: 0,
      statusStr: 'Нет данных'
    },
    {
      id: 'water',
      name: 'Вода',
      keyBase: 'waterLevel',
      valueKind: 'number',
      unit: '%',
      rawValue: null,
      status: 0,
      statusStr: 'Нет данных'
    },
    {
      id: 'level',
      name: 'Уровень',
      keyBase: 'upperLevel',
      valueKind: 'bool',
      unit: '%',
      rawValue: null,
      status: 0,
      statusStr: 'Нет данных'
    },
    {
      id: 'ppm',
      name: 'PPM',
      keyBase: 'ppmMeter',
      valueKind: 'number',
      unit: 'ppm',
      rawValue: null,
      status: 0,
      statusStr: 'Нет данных'
    },
    {
      id: 'ph',
      name: 'PH',
      keyBase: 'phMeter',
      valueKind: 'number',
      unit: 'pH',
      rawValue: null,
      status: 0,
      statusStr: 'Нет данных'
    },
    {
      id: 'purity',
      name: 'Чистота',
      keyBase: 'turbidimeter',
      valueKind: 'number',
      unit: '%',
      rawValue: null,
      status: 0,
      statusStr: 'Нет данных'
    },
    {
      id: 'temp',
      name: 'Temp',
      keyBase: 'temperature',
      valueKind: 'number',
      unit: '°C',
      rawValue: null,
      status: 0,
      statusStr: 'Нет данных'
    },
    {
      id: 'bridge',
      name: 'Мост',
      keyBase: 'bridge',
      valueKind: 'bool',
      rawValue: null,
      status: 0,
      statusStr: 'Нет данных'
    },
    {
      id: 'controller',
      name: 'Контроллер',
      keyBase: 'multiController',
      valueKind: 'bool',
      rawValue: null,
      status: 0,
      statusStr: 'Нет данных'
    },
    {
      id: 'system',
      name: 'Система',
      keyBase: 'system',
      valueKind: 'bool',
      rawValue: null,
      status: 0,
      statusStr: 'Нет данных'
    },
    {
      id: 'reserve',
      name: 'Резерв',
      keyBase: 'reserve',
      valueKind: 'number',
      rawValue: null,
      status: 0,
      statusStr: 'Нет данных'
    }
  ]);

  readonly system = signal<SystemState>({
    mode: 'FLOW',
    nextSwitchTime: '00:12',
    plainType: 'Затопление',
    swingState: ''
  });

  readonly tickerItems = signal<string[]>(['Подключение к телеметрии...']);
  readonly tickerQueue = signal<string[]>([]);
  readonly selectedWidget = signal<WidgetState | null>(null);
  readonly viewMode = signal<'dashboard' | 'settings'>('dashboard');
  readonly selectedDeviceId = signal<string>('pump');
  readonly entryValues = signal<Record<string, unknown>>({});
  readonly entryDrafts = signal<Record<string, unknown>>({});
  readonly entryDirty = signal<Record<string, boolean>>({});

  readonly settingsDevices: SettingsDevice[] = [
    {
      id: 'pump',
      name: 'Насос',
      entries: [
        { key: 'pump.config.enabled', label: 'Активен', section: 'config', type: 'bool' },
        {
          key: 'pump.config.mode',
          label: 'Режим',
          section: 'config',
          type: 'select',
          options: [
            { value: 0, label: 'FLOW' },
            { value: 1, label: 'SWING' },
            { value: 2, label: 'DRIP' }
          ]
        },
        { key: 'pump.config.onTime', label: 'Время работы', section: 'config', type: 'number', unit: 'сек' },
        { key: 'pump.config.offTime', label: 'Пауза', section: 'config', type: 'number', unit: 'сек' },
        { key: 'pump.config.swingTime', label: 'Качели', section: 'config', type: 'number', unit: 'сек' },
        { key: 'pump.config.validTime', label: 'Валидность', section: 'config', type: 'number', unit: 'сек' },
        { key: 'pump.config.maxFloodTime', label: 'Макс. затопление', section: 'config', type: 'number', unit: 'сек' },
        {
          key: 'pump.int.plainType',
          label: 'Тип полива',
          section: 'int',
          type: 'select',
          options: [
            { value: 0, label: 'Осушение' },
            { value: 1, label: 'Затопление' }
          ]
        },
        {
          key: 'pump.int.nextSwitchTime',
          label: 'До переключения',
          section: 'int',
          type: 'number',
          unit: 'сек'
        },
        { key: 'pump.int.desiredState', label: 'Желаемое состояние', section: 'int', type: 'bool' },
        {
          key: 'pump.int.swingState',
          label: 'Отработка качелей',
          section: 'int',
          type: 'select',
          options: [
            { value: 0, label: 'Долив' },
            { value: 1, label: 'Слив' }
          ]
        }
      ]
    },
    {
      id: 'lamp',
      name: 'Лампа',
      entries: [
        { key: 'lamp.config.enabled', label: 'Активна', section: 'config', type: 'bool' },
        { key: 'lamp.config.onTime', label: 'Включение', section: 'config', type: 'time' },
        { key: 'lamp.config.offTime', label: 'Выключение', section: 'config', type: 'time' }
      ]
    },
    {
      id: 'waterLevel',
      name: 'Датчик воды',
      entries: [
        {
          key: 'waterLevel.config.minValue',
          label: 'Минимум',
          section: 'config',
          type: 'number',
          unit: '%'
        }
      ]
    },
    { id: 'ppmMeter', name: 'PPM', entries: [] },
    { id: 'phMeter', name: 'PH', entries: [] },
    { id: 'turbidimeter', name: 'Чистота', entries: [] },
    { id: 'temperature', name: 'Температура', entries: [] },
    { id: 'upperLevel', name: 'Уровень', entries: [] },
    {
      id: 'bridge',
      name: 'Мост',
      entries: [{ key: 'bridge.config.mac', label: 'MAC адреса', section: 'config', type: 'list' }]
    },
    {
      id: 'system',
      name: 'Система',
      entries: [{ key: 'system.config.maintance', label: 'Режим обслуживания', section: 'config', type: 'bool' }]
    },
    { id: 'multiController', name: 'Контроллер', entries: [] }
  ];

  private telemetrySocket?: WebSocket;
  private logSocket?: WebSocket;
  private widgetIndex = new Map<string, number>();
  private settingsEntryIndex = new Map<string, SettingsEntry>();
  private readonly isBrowser: boolean;
  private tickerOffset = 0;
  private tickerFrameId?: number;
  private lastFrameTime = 0;
  private readonly tickerSpeed = 200;
  private readonly tickerQueueLimit = 200;

  @ViewChild('tickerContainer')
  private tickerContainer?: ElementRef<HTMLDivElement>;

  @ViewChild('tickerTrack')
  private tickerTrack?: ElementRef<HTMLDivElement>;

  constructor(@Inject(PLATFORM_ID) platformId: object, private zone: NgZone) {
    this.isBrowser = isPlatformBrowser(platformId);
  }

  ngOnInit(): void {
    this.widgets().forEach((widget, index) => {
      this.widgetIndex.set(widget.keyBase, index);
    });
    this.initializeSettingsIndex();
    if (this.isBrowser) {
      this.connectTelemetry();
      this.connectLogs();
      this.loadSettings();
    }
  }

  ngAfterViewInit(): void {
    if (this.isBrowser) {
      const container = this.tickerContainer?.nativeElement;
      this.tickerOffset = container ? container.clientWidth : 0;
      this.applyTickerTransform();
      this.startTickerLoop();
    }
  }

  ngOnDestroy(): void {
    if (this.isBrowser) {
      this.telemetrySocket?.close();
      this.logSocket?.close();
    }
    if (this.tickerFrameId !== undefined) {
      cancelAnimationFrame(this.tickerFrameId);
    }
  }

  openWidget(widget: WidgetState): void {
    this.selectedWidget.set(widget);
  }

  closeWidget(): void {
    this.selectedWidget.set(null);
  }

  openSettings(): void {
    this.viewMode.set('settings');
    if (this.isBrowser) {
      this.loadSettings();
    }
  }

  closeSettings(): void {
    this.viewMode.set('dashboard');
  }

  selectDevice(deviceId: string): void {
    this.selectedDeviceId.set(deviceId);
  }

  selectedDevice(): SettingsDevice {
    return this.settingsDevices.find((device) => device.id === this.selectedDeviceId()) ??
      this.settingsDevices[0];
  }

  settingsEntries(section: SettingsSection): SettingsEntry[] {
    return this.selectedDevice().entries.filter((entry) => entry.section === section);
  }

  isMaintenanceEnabled(): boolean {
    const value = this.entryValues()['system.config.maintance'];
    return Boolean(value);
  }

  entryValue(key: string): unknown {
    return this.entryValues()[key];
  }

  entryDraftValue(entry: SettingsEntry): string {
    const draft = this.entryDrafts()[entry.key];
    if (entry.type === 'bool') {
      return '';
    }
    if (entry.type === 'time') {
      if (draft === null || draft === undefined || draft === '') {
        return '';
      }
      return this.minutesToTimeString(Number(draft));
    }
    if (draft === null || draft === undefined) {
      return '';
    }
    return String(draft);
  }

  entryDraftChecked(entry: SettingsEntry): boolean {
    const draft = this.entryDrafts()[entry.key];
    return Boolean(draft);
  }

  entryDirtyState(entry: SettingsEntry): boolean {
    return Boolean(this.entryDirty()[entry.key]);
  }

  entryDisabled(entry: SettingsEntry): boolean {
    return entry.section === 'int' && !this.isMaintenanceEnabled();
  }

  onEntryInput(entry: SettingsEntry, event: Event): void {
    const target = event.target as HTMLInputElement | HTMLTextAreaElement | HTMLSelectElement;
    const rawValue = entry.type === 'bool' ? (target as HTMLInputElement).checked : target.value;
    this.entryDrafts.update((drafts) => ({ ...drafts, [entry.key]: rawValue }));
    this.entryDirty.update((dirty) => ({ ...dirty, [entry.key]: true }));
  }

  resetEntry(entry: SettingsEntry): void {
    const value = this.entryValues()[entry.key];
    this.entryDrafts.update((drafts) => ({
      ...drafts,
      [entry.key]: this.normalizeDraft(entry, value)
    }));
    this.entryDirty.update((dirty) => ({ ...dirty, [entry.key]: false }));
  }

  async saveEntry(entry: SettingsEntry): Promise<void> {
    if (this.entryDisabled(entry)) {
      return;
    }
    const draft = this.entryDrafts()[entry.key];
    const value = this.parseDraft(entry, draft);
    const ok = await this.setEntryValue(entry.key, value);
    if (ok) {
      const fresh = await this.fetchEntryValue(entry.key);
      if (fresh !== undefined) {
        this.applyEntryValue(entry.key, fresh, true);
      } else {
        this.applyEntryValue(entry.key, value, true);
      }
    }
  }

  statusClass(status: number): string {
    if (status === 3) {
      return 'status-error';
    }
    if (status === 2) {
      return 'status-working';
    }
    if (status === 1) {
      return 'status-warning';
    }
    return 'status-idle';
  }

  statusLabel(status: number): string {
    if (status === 3) {
      return 'Ошибка';
    }
    if (status === 2) {
      return 'Работает';
    }
    if (status === 1) {
      return 'Предупреждение';
    }
    return 'Нет данных';
  }

  formatValue(widget: WidgetState): string {
    if (widget.rawValue === null || widget.rawValue === undefined) {
      return '--';
    }
    if (widget.valueKind === 'bool') {
      return widget.rawValue ? 'ON' : 'OFF';
    }
    const numeric = Number(widget.rawValue);
    if (Number.isNaN(numeric)) {
      return `${widget.rawValue}`;
    }
    const formatted = numeric % 1 === 0 ? numeric.toFixed(0) : numeric.toFixed(2);
    return widget.unit ? `${formatted} ${widget.unit}` : formatted;
  }

  isFlooding(): boolean {
    const state = this.system().plainType.toLowerCase();
    return state.includes('затоп') || state.includes('fill');
  }

  trackById(_index: number, widget: WidgetState): string {
    return widget.id;
  }

  private connectTelemetry(): void {
    if (!this.isBrowser) {
      return;
    }
    const protocol = location.protocol === 'https:' ? 'wss' : 'ws';
    this.telemetrySocket = new WebSocket(`${protocol}://${location.host}/ws/telemetry`);
    this.telemetrySocket.addEventListener('message', (event) => {
      try {
        const payload = JSON.parse(event.data as string);
        if (payload?.type === 'telemetry') {
          this.handleTelemetry(payload.key, payload.value);
        }
      } catch {
        // Ignore malformed telemetry payloads
      }
    });
  }

  private connectLogs(): void {
    if (!this.isBrowser) {
      return;
    }
    const protocol = location.protocol === 'https:' ? 'wss' : 'ws';
    this.logSocket = new WebSocket(`${protocol}://${location.host}/ws/logs`);
    this.logSocket.addEventListener('message', (event) => {
      const message = (event.data ?? '').toString().trim();
      if (message) {
        this.handleTickerMessage(message);
      }
    });
  }

  private handleTelemetry(key: string, value: unknown): void {
    if (!key) {
      return;
    }

    if (key.endsWith('.telem.value')) {
      const base = key.replace('.telem.value', '');
      this.updateWidget(base, { rawValue: value });
      return;
    }

    if (key.endsWith('.telem.status')) {
      const base = key.replace('.telem.status', '');
      const numeric = Number(value);
      const status = Number.isNaN(numeric) ? 0 : (numeric as StatusLevel);
      this.updateWidget(base, { status });
      return;
    }

    if (key.endsWith('.telem.statusStr')) {
      const base = key.replace('.telem.statusStr', '');
      this.updateWidget(base, { statusStr: `${value ?? ''}`.trim() || 'Нет данных' });
      return;
    }

    if (key === 'pump.config.mode') {
      this.system.update((current) => ({
        ...current,
        mode: this.formatPumpMode(value)
      }));
      this.applyEntryValue(key, value, false);
      return;
    }

    if (key === 'pump.int.nextSwitchTime') {
      this.system.update((current) => ({
        ...current,
        nextSwitchTime: this.formatSeconds(value)
      }));
      this.applyEntryValue(key, value, false);
      return;
    }

    if (key === 'pump.int.plainType') {
      this.system.update((current) => ({
        ...current,
        plainType: this.formatPlainType(value)
      }));
      this.applyEntryValue(key, value, false);
      return;
    }

    if (key === 'pump.int.swingState') {
      this.system.update((current) => ({
        ...current,
        swingState: this.formatSwingState(value)
      }));
      this.applyEntryValue(key, value, false);
      return;
    }

    if (this.settingsEntryIndex.has(key)) {
      this.applyEntryValue(key, value, false);
    }

  }

  private updateWidget(base: string, patch: Partial<WidgetState>): void {
    const index = this.widgetIndex.get(base);
    if (index === undefined) {
      return;
    }
    this.widgets.update((list) => {
      const next = [...list];
      next[index] = { ...next[index], ...patch };
      return next;
    });
  }

  private initializeSettingsIndex(): void {
    this.settingsDevices.forEach((device) => {
      device.entries.forEach((entry) => {
        this.settingsEntryIndex.set(entry.key, entry);
      });
    });
  }

  private loadSettings(): void {
    const keys = Array.from(this.settingsEntryIndex.keys());
    keys.forEach((key) => {
      this.fetchEntryValue(key).then((value) => {
        if (value !== undefined && value !== null) {
          this.applyEntryValue(key, value, true);
        }
      });
    });
  }

  private applyEntryValue(key: string, value: unknown, updateDraft: boolean): void {
    this.entryValues.update((values) => ({ ...values, [key]: value }));
    const isDirty = Boolean(this.entryDirty()[key]);
    if (updateDraft || !isDirty) {
      const entry = this.settingsEntryIndex.get(key);
      const normalized = entry ? this.normalizeDraft(entry, value) : value;
      this.entryDrafts.update((drafts) => ({ ...drafts, [key]: normalized }));
      this.entryDirty.update((dirty) => ({ ...dirty, [key]: false }));
    }
  }

  private normalizeDraft(entry: SettingsEntry, value: unknown): unknown {
    if (entry.type === 'list') {
      if (Array.isArray(value)) {
        return value.join('\n');
      }
      return value ?? '';
    }
    if (entry.type === 'bool') {
      return Boolean(value);
    }
    if (entry.type === 'time') {
      if (value === null || value === undefined || value === '') {
        return '';
      }
      const numeric = Number(value);
      return Number.isNaN(numeric) ? '' : numeric;
    }
    if (value === null || value === undefined) {
      return '';
    }
    return value;
  }

  private parseDraft(entry: SettingsEntry, value: unknown): unknown {
    if (entry.type === 'bool') {
      return Boolean(value);
    }
    if (entry.type === 'number') {
      const numeric = Number(value);
      return Number.isNaN(numeric) ? 0 : numeric;
    }
    if (entry.type === 'select') {
      const numeric = Number(value);
      return Number.isNaN(numeric) ? 0 : numeric;
    }
    if (entry.type === 'time') {
      if (typeof value === 'string') {
        return this.timeStringToMinutes(value);
      }
      const numeric = Number(value);
      return Number.isNaN(numeric) ? 0 : numeric;
    }
    if (entry.type === 'list') {
      if (Array.isArray(value)) {
        return value;
      }
      const text = String(value ?? '');
      return text
        .split('\n')
        .map((item) => item.trim())
        .filter(Boolean);
    }
    return value;
  }

  private async fetchEntryValue(key: string): Promise<unknown | undefined> {
    try {
      const response = await fetch(`/entry?q=${encodeURIComponent(key)}`);
      if (!response.ok) {
        return undefined;
      }
      const contentType = response.headers.get('content-type') ?? '';
      if (contentType.includes('application/json')) {
        const data = await response.json();
        if (data && typeof data === 'object') {
          if ('value' in (data as Record<string, unknown>)) {
            const val = (data as { value: unknown }).value;
            return val === null ? undefined : val;
          }
          if ('key' in (data as Record<string, unknown>) && 'value' in (data as Record<string, unknown>)) {
            const typed = data as { key: string; value: unknown };
            if (typed.key === key) {
              return typed.value === null ? undefined : typed.value;
            }
          }
          if (key in (data as Record<string, unknown>)) {
            const val = (data as Record<string, unknown>)[key];
            return val === null ? undefined : val;
          }
        }
        return data;
      }
      const text = await response.text();
      const trimmed = text.trim();
      if (trimmed.startsWith('{') || trimmed.startsWith('[')) {
        try {
          const data = JSON.parse(trimmed) as Record<string, unknown>;
          if (data && typeof data === 'object' && key in data) {
            return data[key];
          }
          return data;
        } catch {
          return trimmed;
        }
      }
      return trimmed;
    } catch {
      return undefined;
    }
  }

  private async setEntryValue(key: string, value: unknown): Promise<boolean> {
    try {
      const payload: Record<string, unknown> = { [key]: value };
      const response = await fetch('/entry', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      return response.ok;
    } catch {
      return false;
    }
  }

  private handleTickerMessage(message: string): void {
    const container = this.tickerContainer?.nativeElement;
    const track = this.tickerTrack?.nativeElement;
    if (!container || !track) {
      this.tickerQueue.update((queue) => {
        const next = [...queue, message];
        return next.length > this.tickerQueueLimit
          ? next.slice(next.length - this.tickerQueueLimit)
          : next;
      });
      return;
    }
    if (!this.tickerItems().length) {
      this.tickerItems.set([message]);
      this.tickerOffset = container.clientWidth;
      this.applyTickerTransform();
      return;
    }
    if (this.canAppendMessage(container, track)) {
      this.tickerItems.update((items) => [...items, message]);
      return;
    }
    this.tickerQueue.update((queue) => {
      const next = [...queue, message];
      return next.length > this.tickerQueueLimit
        ? next.slice(next.length - this.tickerQueueLimit)
        : next;
    });
  }

  private startTickerLoop(): void {
    this.zone.runOutsideAngular(() => {
      const step = (timestamp: number) => {
        if (!this.lastFrameTime) {
          this.lastFrameTime = timestamp;
        }
        const deltaSeconds = (timestamp - this.lastFrameTime) / 1000;
        this.lastFrameTime = timestamp;
        this.advanceTicker(deltaSeconds);
        this.tickerFrameId = requestAnimationFrame(step);
      };
      this.tickerFrameId = requestAnimationFrame(step);
    });
  }

  private advanceTicker(deltaSeconds: number): void {
    const container = this.tickerContainer?.nativeElement;
    const track = this.tickerTrack?.nativeElement;
    if (!container || !track) {
      return;
    }
    if (!this.tickerItems().length) {
      this.flushTickerQueue(container, track);
      if (!this.tickerItems().length) {
        return;
      }
    }
    this.tickerOffset -= this.tickerSpeed * deltaSeconds;
    this.applyTickerTransform();
    this.flushTickerQueue(container, track);

    const trackWidth = track.scrollWidth;
    if (this.tickerOffset + trackWidth < 0) {
      this.tickerItems.set([]);
      this.tickerOffset = container.clientWidth;
      this.applyTickerTransform();
      this.flushTickerQueue(container, track);
    }
  }

  private flushTickerQueue(container: HTMLDivElement, track: HTMLDivElement): void {
    let queue = this.tickerQueue();
    if (!queue.length) {
      return;
    }
    if (!this.canAppendMessage(container, track)) {
      return;
    }
    this.tickerItems.update((items) => [...items, ...queue]);
    this.tickerQueue.set([]);
  }

  private canAppendMessage(container: HTMLDivElement, track: HTMLDivElement): boolean {
    const rightEdge = this.tickerOffset + track.scrollWidth;
    return rightEdge >= container.clientWidth;
  }

  private applyTickerTransform(): void {
    const track = this.tickerTrack?.nativeElement;
    if (!track) {
      return;
    }
    track.style.transform = `translateX(${this.tickerOffset}px)`;
  }

  private formatPumpMode(value: unknown): string {
    const numeric = Number(value);
    if (numeric === 0) {
      return 'FLOW';
    }
    if (numeric === 1) {
      return 'SWING';
    }
    if (numeric === 2) {
      return 'DRIP';
    }
    return String(value ?? '');
  }

  private formatPlainType(value: unknown): string {
    const numeric = Number(value);
    if (numeric === 0) {
      return 'Осушение';
    }
    if (numeric === 1) {
      return 'Затопление';
    }
    return String(value ?? '');
  }

  private formatSwingState(value: unknown): string {
    const numeric = Number(value);
    if (numeric === 0) {
      return 'Долив';
    }
    if (numeric === 1) {
      return 'Слив';
    }
    return String(value ?? '');
  }

  private formatSeconds(value: unknown): string {
    const numeric = Number(value);
    if (!Number.isFinite(numeric)) {
      return '--:--';
    }
    const safe = Math.max(0, Math.floor(numeric));
    const minutes = Math.floor(safe / 60);
    const seconds = safe % 60;
    return `${minutes.toString().padStart(2, '0')}:${seconds
      .toString()
      .padStart(2, '0')}`;
  }

  private minutesToTimeString(value: number): string {
    if (!Number.isFinite(value)) {
      return '';
    }
    const safe = Math.max(0, Math.floor(value));
    const hours = Math.floor(safe / 60) % 24;
    const minutes = safe % 60;
    return `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}`;
  }

  private timeStringToMinutes(value: string): number {
    const parts = value.split(':').map((part) => part.trim());
    if (parts.length !== 2) {
      return 0;
    }
    const hours = Number(parts[0]);
    const minutes = Number(parts[1]);
    if (!Number.isFinite(hours) || !Number.isFinite(minutes)) {
      return 0;
    }
    const clampedHours = Math.min(Math.max(0, Math.floor(hours)), 23);
    const clampedMinutes = Math.min(Math.max(0, Math.floor(minutes)), 59);
    return clampedHours * 60 + clampedMinutes;
  }
}
