//go:build windows

package main

// Windows service mode for nekobox_core:
//
//	nekobox_core.exe service run      (started by the SCM, or from a console for debugging)
//	nekobox_core.exe service status   (prints the status file written by the service)
//
// The service hosts the normal Thrift RPC server on a Unix-domain socket whose
// directory ACL only admits SYSTEM, Administrators and the accounts listed in
// service.json. The GUI attaches to that socket instead of spawning its own core.
//
// Every successful Start RPC is saved (DPAPI, machine scope, in an ACL-protected
// directory) together with an "enabled" flag. At boot the supervisor replays the
// last saved request, so the exact config built by the GUI's ConfigBuilder is
// used, including extra cores. A Stop RPC clears the flag; stopping the service
// (shutdown, sc stop) does not.

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"

	"golang.org/x/sys/windows"
	"golang.org/x/sys/windows/svc"
	"golang.org/x/sys/windows/svc/debug"

	"nekobox_core/gen"
	"nekobox_core/internal"
)

const (
	svcName = "NekoBoxCore"
	// Present in the binary so install scripts can tell a service-capable build
	// from a stock one without executing it.
	serviceBuildMarker    = "NEKOBOX-SERVICE-MODE-V1"
	pbtApmResumeSuspend   = 0x7
	pbtApmResumeAutomatic = 0x12

	sddlPrivate = "D:PAI(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)"

	cryptProtectUIForbidden  = 0x1
	cryptProtectLocalMachine = 0x4
)

type serviceConfig struct {
	// Accounts (DOMAIN\name, name, or a SID string) allowed to talk to the core.
	AllowedUsers []string `json:"allowed_users"`
	// Where rule sets are cached. Default: <ProgramData>\NekoBoxService\rule_sets
	RulesetDir string `json:"ruleset_dir"`
	// Extra directories an extra core executable may live in (install dir is always allowed).
	ExtraCoreDirs   []string `json:"extra_core_dirs"`
	Debug           bool     `json:"debug"`
	WatchdogSeconds int      `json:"watchdog_seconds"`
}

type statusFile struct {
	Version     int    `json:"version"`
	State       string `json:"state"` // starting | running | stopped
	ServicePID  int    `json:"service_pid"`
	CoreRunning bool   `json:"core_running"`
	Enabled     bool   `json:"enabled"`
	Tun         string `json:"tun,omitempty"`
	TunUp       bool   `json:"tun_up"`
	LastError   string `json:"last_error,omitempty"`
	Socket      string `json:"socket"`
	Updated     int64  `json:"updated"`
}

type snapshotFile struct {
	Req   *gen.LoadConfigReq `json:"req"`
	Hash  string             `json:"hash"`
	Saved time.Time          `json:"saved"`
}

type svcRuntime struct {
	mu         sync.Mutex
	cfg        serviceConfig
	base       string
	ipc        string
	installDir string
	sock       string

	enabled   bool
	hash      string
	tunName   string
	lastErr   string
	state     string
	nextTry   time.Time
	resumed   atomic.Bool
	kick      chan struct{}
	logFile   *os.File
	isService bool
}

var rt = &svcRuntime{kick: make(chan struct{}, 1), state: "starting"}

func programData() string {
	p, err := windows.KnownFolderPath(windows.FOLDERID_ProgramData, 0)
	if err != nil || p == "" {
		return `C:\ProgramData`
	}
	return p
}

func applyDACL(path, sddl string) error {
	sd, err := windows.SecurityDescriptorFromString(sddl)
	if err != nil {
		return err
	}
	dacl, _, err := sd.DACL()
	if err != nil {
		return err
	}
	return windows.SetNamedSecurityInfo(path, windows.SE_FILE_OBJECT,
		windows.DACL_SECURITY_INFORMATION|windows.PROTECTED_DACL_SECURITY_INFORMATION,
		nil, nil, dacl, nil)
}

func ensureDir(path, sddl string) error {
	if err := os.MkdirAll(path, 0o700); err != nil {
		return err
	}
	return applyDACL(path, sddl)
}

func resolveSID(name string) (string, error) {
	if strings.HasPrefix(strings.ToUpper(name), "S-1-") {
		return name, nil
	}
	sid, _, _, err := windows.LookupSID("", name)
	if err != nil {
		return "", err
	}
	return sid.String(), nil
}

// ---------- DPAPI (machine scope) ----------

func dpapiProtect(b []byte) ([]byte, error) {
	if len(b) == 0 {
		return nil, errors.New("empty data")
	}
	in := windows.DataBlob{Size: uint32(len(b)), Data: &b[0]}
	var out windows.DataBlob
	err := windows.CryptProtectData(&in, nil, nil, 0, nil,
		cryptProtectLocalMachine|cryptProtectUIForbidden, &out)
	if err != nil {
		return nil, err
	}
	defer windows.LocalFree(windows.Handle(unsafe.Pointer(out.Data)))
	res := make([]byte, out.Size)
	copy(res, unsafe.Slice(out.Data, out.Size))
	return res, nil
}

func dpapiUnprotect(b []byte) ([]byte, error) {
	if len(b) == 0 {
		return nil, errors.New("empty data")
	}
	in := windows.DataBlob{Size: uint32(len(b)), Data: &b[0]}
	var out windows.DataBlob
	err := windows.CryptUnprotectData(&in, nil, nil, 0, nil, cryptProtectUIForbidden, &out)
	if err != nil {
		return nil, err
	}
	defer windows.LocalFree(windows.Handle(unsafe.Pointer(out.Data)))
	res := make([]byte, out.Size)
	copy(res, unsafe.Slice(out.Data, out.Size))
	return res, nil
}

// ---------- persistence ----------

func (r *svcRuntime) statePath(name string) string { return filepath.Join(r.base, "state", name) }

func writeFileAtomic(path string, data []byte) error {
	tmp := path + ".tmp"
	if err := os.WriteFile(tmp, data, 0o600); err != nil {
		return err
	}
	if err := os.Rename(tmp, path); err != nil {
		// a reader may hold the destination open without FILE_SHARE_DELETE
		_ = os.Remove(tmp)
		return os.WriteFile(path, data, 0o600)
	}
	return nil
}

func (r *svcRuntime) saveSnapshot(req *gen.LoadConfigReq, hash string) error {
	raw, err := json.Marshal(snapshotFile{Req: req, Hash: hash, Saved: time.Now()})
	if err != nil {
		return err
	}
	enc, err := dpapiProtect(raw)
	if err != nil {
		return err
	}
	return writeFileAtomic(r.statePath("snapshot.bin"), enc)
}

func (r *svcRuntime) loadSnapshot() (*gen.LoadConfigReq, string, error) {
	enc, err := os.ReadFile(r.statePath("snapshot.bin"))
	if err != nil {
		return nil, "", err
	}
	raw, err := dpapiUnprotect(enc)
	if err != nil {
		return nil, "", err
	}
	var s snapshotFile
	if err := json.Unmarshal(raw, &s); err != nil {
		return nil, "", err
	}
	if s.Req == nil {
		return nil, "", errors.New("snapshot has no request")
	}
	return s.Req, s.Hash, nil
}

func (r *svcRuntime) setEnabledLocked(v bool) {
	r.enabled = v
	data, _ := json.Marshal(map[string]bool{"enabled": v})
	if err := writeFileAtomic(r.statePath("control.json"), data); err != nil {
		log.Printf("service: cannot persist enabled flag: %v", err)
	}
}

func (r *svcRuntime) loadEnabled() bool {
	data, err := os.ReadFile(r.statePath("control.json"))
	if err != nil {
		return false
	}
	var m map[string]bool
	if json.Unmarshal(data, &m) != nil {
		return false
	}
	return m["enabled"]
}

func (r *svcRuntime) writeStatusLocked() {
	st := statusFile{
		Version:     1,
		State:       r.state,
		ServicePID:  os.Getpid(),
		CoreRunning: internal.BoxInstance != nil,
		Enabled:     r.enabled,
		Tun:         r.tunName,
		LastError:   r.lastErr,
		Socket:      r.sock,
		Updated:     time.Now().Unix(),
	}
	if r.tunName != "" {
		st.TunUp = ifaceUp(r.tunName)
	}
	data, _ := json.MarshalIndent(st, "", "  ")
	if err := writeFileAtomic(filepath.Join(r.ipc, "status.json"), data); err != nil {
		log.Printf("service: cannot write status: %v", err)
	}
}

// ---------- request handling ----------

func hashRequest(in *gen.LoadConfigReq) string {
	h := sha256.New()
	w := func(s string) { _, _ = io.WriteString(h, s); _, _ = h.Write([]byte{0}) }
	w(in.CoreConfig)
	w(fmt.Sprint(in.DisableStats, in.NeedExtraProcess, in.ExtraNoOut))
	w(in.ExtraProcessPath)
	w(in.ExtraProcessArgs_)
	w(in.ExtraProcessConf)
	w(in.ExtraProcessConfDir)
	return hex.EncodeToString(h.Sum(nil))
}

func tunNameFromConfig(coreConfig string) string {
	var cfg struct {
		Inbounds []map[string]any `json:"inbounds"`
	}
	if json.Unmarshal([]byte(coreConfig), &cfg) != nil {
		return ""
	}
	for _, in := range cfg.Inbounds {
		if t, _ := in["type"].(string); t == "tun" {
			if n, _ := in["interface_name"].(string); n != "" {
				return n
			}
		}
	}
	return ""
}

func ifaceUp(name string) bool {
	ifc, err := net.InterfaceByName(name)
	return err == nil && ifc.Flags&net.FlagUp != 0
}

func (r *svcRuntime) pathAllowed(p string) bool {
	lp := strings.ToLower(filepath.Clean(p))
	dirs := append([]string{r.installDir}, r.cfg.ExtraCoreDirs...)
	for _, d := range dirs {
		if d == "" {
			continue
		}
		ld := strings.ToLower(filepath.Clean(d))
		if strings.HasPrefix(lp, ld+`\`) {
			return true
		}
	}
	return false
}

// sanitize applies defense-in-depth limits to requests arriving over RPC. The
// callers are already restricted by the socket ACL; these checks only stop a
// permitted account from using the privileged core for arbitrary file writes or
// arbitrary executables.
func (r *svcRuntime) sanitize(in *gen.LoadConfigReq) error {
	if strings.TrimSpace(in.CoreConfig) == "" {
		return errors.New("empty core_config")
	}
	var cfg map[string]any
	if err := json.Unmarshal([]byte(in.CoreConfig), &cfg); err != nil {
		return fmt.Errorf("invalid core_config: %w", err)
	}
	if lg, ok := cfg["log"].(map[string]any); ok {
		if out, _ := lg["output"].(string); out != "" {
			return errors.New("service mode: log.output is not allowed")
		}
	}
	if !in.NeedExtraProcess {
		return nil
	}
	p := in.ExtraProcessPath
	if p == "" {
		return errors.New("extra core path is empty")
	}
	if !filepath.IsAbs(p) {
		p = filepath.Join(r.installDir, p)
	}
	p = filepath.Clean(p)
	if _, err := os.Stat(p); err != nil && filepath.Ext(p) == "" {
		p += ".exe"
	}
	rp, err := filepath.EvalSymlinks(p)
	if err != nil {
		return fmt.Errorf("extra core not found (%s): %v", p, err)
	}
	if !r.pathAllowed(rp) {
		return fmt.Errorf("extra core %q is outside the allowed directories (install dir / extra_core_dirs in service.json)", rp)
	}
	in.ExtraProcessPath = rp
	in.ExtraProcessConfDir = filepath.Join(r.base, "extra")
	return nil
}

func (s *server) serviceStart(ctx context.Context, in *gen.LoadConfigReq) (*gen.ErrorResp, error) {
	rt.mu.Lock()
	defer rt.mu.Unlock()
	out := new(gen.ErrorResp)

	if err := rt.sanitize(in); err != nil {
		out.Error = err.Error()
		return out, nil
	}
	h := hashRequest(in)

	if internal.BoxInstance != nil {
		if h == rt.hash {
			// The GUI attached and asked for the profile that is already running.
			log.Printf("service: Start with identical config, keeping running instance")
			rt.lastErr = ""
			rt.setEnabledLocked(true)
			rt.writeStatusLocked()
			return out, nil
		}
		log.Printf("service: replacing running profile")
		if r, _ := s.stopInternal(ctx, &gen.EmptyReq{}); r != nil && r.Error != "" {
			log.Printf("service: stop before replace: %s", r.Error)
		}
		rt.hash = ""
	}

	resp, err := s.startInternal(ctx, in)
	if err != nil {
		out.Error = err.Error()
		rt.lastErr = out.Error
		return out, nil
	}
	if resp.Error != "" {
		rt.lastErr = resp.Error
		rt.writeStatusLocked()
		return resp, nil
	}

	rt.hash = h
	rt.tunName = tunNameFromConfig(in.CoreConfig)
	rt.lastErr = ""
	if err := rt.saveSnapshot(in, h); err != nil {
		rt.lastErr = "profile running but snapshot not saved (no autostart at boot): " + err.Error()
		log.Printf("service: %s", rt.lastErr)
	}
	rt.setEnabledLocked(true)
	rt.writeStatusLocked()
	log.Printf("service: profile started (tun=%q)", rt.tunName)
	return resp, nil
}

func (s *server) serviceStop(ctx context.Context, in *gen.EmptyReq) (*gen.ErrorResp, error) {
	rt.mu.Lock()
	defer rt.mu.Unlock()
	resp, err := s.stopInternal(ctx, in)
	rt.hash = ""
	rt.setEnabledLocked(false) // user disconnected: do not autostart at next boot
	rt.writeStatusLocked()
	log.Printf("service: profile stopped by client, autostart disabled until next Start")
	return resp, err
}

// ---------- supervisor ----------

func (r *svcRuntime) check(backoff *time.Duration, tunMissing *int) {
	r.mu.Lock()
	defer r.mu.Unlock()
	defer func() {
		r.state = "running"
		r.writeStatusLocked()
	}()

	if !r.enabled {
		*tunMissing = 0
		return
	}
	srv := &server{}

	if internal.BoxInstance != nil {
		threshold := 3
		if r.resumed.Swap(false) {
			threshold = 1
		}
		if r.tunName != "" && !ifaceUp(r.tunName) {
			*tunMissing++
			if *tunMissing < threshold {
				return
			}
			log.Printf("watchdog: TUN %q is down, restarting the core", r.tunName)
			if resp, _ := srv.stopInternal(context.Background(), &gen.EmptyReq{}); resp != nil && resp.Error != "" {
				log.Printf("watchdog: stop: %s", resp.Error)
			}
			r.hash = ""
			*tunMissing = 0
		} else {
			*tunMissing = 0
			*backoff = 3 * time.Second
			return
		}
	}

	if time.Now().Before(r.nextTry) {
		return
	}
	req, h, err := r.loadSnapshot()
	if err != nil {
		r.lastErr = "cannot load snapshot: " + err.Error()
		r.nextTry = time.Now().Add(30 * time.Second)
		log.Printf("watchdog: %s", r.lastErr)
		return
	}
	if err := r.sanitize(req); err != nil {
		r.lastErr = "snapshot rejected: " + err.Error()
		r.nextTry = time.Now().Add(60 * time.Second)
		log.Printf("watchdog: %s", r.lastErr)
		return
	}
	log.Printf("watchdog: starting saved profile")
	errStr := ""
	resp, err := srv.startInternal(context.Background(), req)
	if err != nil {
		errStr = err.Error()
	} else if resp.Error != "" {
		errStr = resp.Error
	}
	if errStr != "" {
		r.lastErr = errStr
		r.nextTry = time.Now().Add(*backoff)
		log.Printf("watchdog: start failed: %s (retry in %v)", errStr, *backoff)
		if *backoff < 60*time.Second {
			*backoff *= 2
		}
		return
	}
	r.hash = h
	r.tunName = tunNameFromConfig(req.CoreConfig)
	r.lastErr = ""
	*backoff = 3 * time.Second
	log.Printf("watchdog: profile started (tun=%q)", r.tunName)
}

func (r *svcRuntime) supervise(ctx context.Context) {
	interval := time.Duration(r.cfg.WatchdogSeconds) * time.Second
	if interval < 3*time.Second {
		interval = 10 * time.Second
	}
	t := time.NewTicker(interval)
	defer t.Stop()
	backoff := 3 * time.Second
	tunMissing := 0
	for {
		r.check(&backoff, &tunMissing)
		select {
		case <-ctx.Done():
			return
		case <-t.C:
		case <-r.kick:
		}
	}
}

// ---------- lifecycle ----------

func (r *svcRuntime) setupLogging(isService bool) error {
	dir := filepath.Join(r.base, "logs")
	if err := os.MkdirAll(dir, 0o700); err != nil {
		return err
	}
	p := filepath.Join(dir, "service.log")
	if fi, err := os.Stat(p); err == nil && fi.Size() > 10<<20 {
		_ = os.Rename(p, p+".1")
	}
	f, err := os.OpenFile(p, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0o600)
	if err != nil {
		return err
	}
	r.logFile = f
	log.SetFlags(log.LstdFlags)
	if isService {
		os.Stdout = f
		os.Stderr = f
		_ = windows.SetStdHandle(windows.STD_OUTPUT_HANDLE, windows.Handle(f.Fd()))
		_ = windows.SetStdHandle(windows.STD_ERROR_HANDLE, windows.Handle(f.Fd()))
		log.SetOutput(f)
	} else {
		log.SetOutput(io.MultiWriter(os.Stderr, f))
	}
	return nil
}

// bootstrap prepares directories, ACLs, environment and starts the RPC server
// and the supervisor. It returns a channel that closes if the RPC server dies.
func (r *svcRuntime) bootstrap(ctx context.Context, isService bool) (<-chan struct{}, error) {
	pd := programData()
	r.base = filepath.Join(pd, "NekoBoxService")
	r.ipc = filepath.Join(pd, "NekoBoxService-ipc")
	r.isService = isService
	exe, err := os.Executable()
	if err != nil {
		return nil, err
	}
	r.installDir = filepath.Dir(exe)

	if err := ensureDir(r.base, sddlPrivate); err != nil {
		return nil, fmt.Errorf("prepare %s: %w", r.base, err)
	}
	if err := r.setupLogging(isService); err != nil {
		return nil, fmt.Errorf("logging: %w", err)
	}
	log.Printf("=== NekoBox core service starting (pid %d, exe %s, %s) ===", os.Getpid(), exe, serviceBuildMarker)

	if data, err := os.ReadFile(filepath.Join(r.base, "service.json")); err == nil {
		data = []byte(strings.TrimPrefix(string(data), "\ufeff"))
		if err := json.Unmarshal(data, &r.cfg); err != nil {
			return nil, fmt.Errorf("service.json: %w", err)
		}
	} else {
		log.Printf("service.json not found, using defaults (no GUI user will be allowed to connect)")
	}

	ipcSDDL := sddlPrivate
	for _, u := range r.cfg.AllowedUsers {
		sid, err := resolveSID(u)
		if err != nil {
			log.Printf("allowed_users: cannot resolve %q: %v", u, err)
			continue
		}
		ipcSDDL += fmt.Sprintf("(A;OICI;FRFWFX;;;%s)", sid)
	}
	// Rebuild the IPC dir ACL on every start so a changed allowed_users takes effect.
	if err := ensureDir(r.ipc, ipcSDDL); err != nil {
		return nil, fmt.Errorf("prepare %s: %w", r.ipc, err)
	}
	for _, d := range []string{"state", "work", "extra", "rule_sets"} {
		if err := os.MkdirAll(filepath.Join(r.base, d), 0o700); err != nil {
			return nil, err
		}
	}
	work := filepath.Join(r.base, "work")
	if err := os.Chdir(work); err != nil {
		return nil, err
	}
	rulesets := r.cfg.RulesetDir
	if rulesets == "" {
		rulesets = filepath.Join(r.base, "rule_sets")
	}
	_ = os.MkdirAll(rulesets, 0o700)
	_ = os.Setenv("InTheNameOf", "Iblis")
	_ = os.Setenv("NEKOBOX_RULESET_CACHE_DIRECTORY", rulesets)
	internal.SetRulesetCachedir(rulesets)
	internal.Debug = r.cfg.Debug

	testCtx, cancelTests = context.WithCancel(context.Background())
	serviceMode = true

	r.enabled = r.loadEnabled()
	log.Printf("autostart enabled=%v", r.enabled)

	r.sock = filepath.Join(r.ipc, "core.sock")
	_ = os.Remove(r.sock)

	r.mu.Lock()
	r.state = "starting"
	r.writeStatusLocked()
	r.mu.Unlock()

	dead := make(chan struct{})
	go func() {
		dbg := r.cfg.Debug
		RunCore(&net.UnixAddr{Name: r.sock, Net: "unix"}, &dbg)
		log.Printf("RPC server exited")
		close(dead)
	}()
	go r.supervise(ctx)
	return dead, nil
}

func (r *svcRuntime) shutdown() {
	r.mu.Lock()
	defer r.mu.Unlock()
	log.Printf("service: shutting down core (autostart flag is kept: %v)", r.enabled)
	r.state = "stopped"
	if internal.BoxInstance != nil {
		if resp, _ := (&server{}).stopInternal(context.Background(), &gen.EmptyReq{}); resp != nil && resp.Error != "" {
			log.Printf("service: stop: %s", resp.Error)
		}
	}
	r.writeStatusLocked()
	_ = os.Remove(r.sock)
}

type nbHandler struct{ isService bool }

func (h *nbHandler) Execute(_ []string, req <-chan svc.ChangeRequest, st chan<- svc.Status) (bool, uint32) {
	const accepted = svc.AcceptStop | svc.AcceptShutdown | svc.AcceptPowerEvent
	st <- svc.Status{State: svc.StartPending}
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	dead, err := rt.bootstrap(ctx, h.isService)
	if err != nil {
		if rt.logFile != nil {
			log.Printf("bootstrap failed: %v", err)
		} else {
			fmt.Fprintf(os.Stderr, "bootstrap failed: %v\n", err)
		}
		return true, 1
	}
	st <- svc.Status{State: svc.Running, Accepts: accepted}

	for {
		select {
		case c := <-req:
			switch c.Cmd {
			case svc.Interrogate:
				st <- c.CurrentStatus
			case svc.Stop, svc.Shutdown:
				st <- svc.Status{State: svc.StopPending}
				cancel()
				rt.shutdown()
				return false, 0
			case svc.PowerEvent:
				if c.EventType == pbtApmResumeAutomatic || c.EventType == pbtApmResumeSuspend {
					log.Printf("service: resume from sleep, scheduling health check")
					go func() {
						time.Sleep(8 * time.Second)
						rt.resumed.Store(true)
						select {
						case rt.kick <- struct{}{}:
						default:
						}
					}()
				}
			}
		case <-dead:
			st <- svc.Status{State: svc.StopPending}
			cancel()
			rt.shutdown()
			return true, 2 // non-zero: lets SCM failure actions restart us
		}
	}
}

func RunServiceMode(args []string) {
	cmd := "run"
	if len(args) > 0 {
		cmd = args[0]
	}
	switch cmd {
	case "run":
		isSvc, err := svc.IsWindowsService()
		if err != nil {
			fmt.Fprintf(os.Stderr, "cannot detect session type: %v\n", err)
			os.Exit(1)
		}
		h := &nbHandler{isService: isSvc}
		if isSvc {
			err = svc.Run(svcName, h)
		} else {
			err = debug.Run(svcName, h)
		}
		if err != nil {
			fmt.Fprintf(os.Stderr, "service failed: %v\n", err)
			os.Exit(1)
		}
		os.Exit(0)
	case "status":
		data, err := os.ReadFile(filepath.Join(programData(), "NekoBoxService-ipc", "status.json"))
		if err != nil {
			fmt.Println("service status unavailable:", err)
			os.Exit(1)
		}
		fmt.Println(string(data))
	default:
		fmt.Println("usage: nekobox_core service [run|status]")
		os.Exit(2)
	}
}
