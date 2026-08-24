# Oak OS

Oak OS e um sistema operacional x86_64 em desenvolvimento. Seu kernel se chama
**Acorn** e a base do projeto e majoritariamente escrita em C, com Assembly
apenas para a entrada de boot e a transicao inicial para long mode.

## Estado atual

Esta primeira etapa implementa uma base real e inicializavel:

- kernel ELF64 freestanding;
- cabecalho Multiboot carregavel pelo GRUB;
- transicao para x86_64 com tabelas de paginas identity-mapped;
- entrada em C com validacao do magic Multiboot;
- saida serial COM1, adequada para QEMU `-nographic`;
- saida VGA texto como verificacao visual inicial;
- IDT x86_64 com 256 gates e stub de excecao com panic serial;
- bitmap de frames fisicos de 4 KiB baseado no memory map Multiboot;
- heap inicial do kernel com alinhamento, limite e self-test;
- PIC remapeado, PIT em aproximadamente 1 kHz e handler de IRQ 0;
- driver de teclado PS/2 no IRQ 1, com traducao Set 1, Shift e buffer circular;
- handlers IRQ preservando registradores gerais antes de chamar C;
- frame de syscall preservando e restaurando `RAX` corretamente no retorno `iretq`;
- scheduler round-robin com TCBs, estados de thread e self-test;
- stacks independentes e troca cooperativa de contexto em Assembly;
- preempcao por IRQ 0 do PIT com preservacao de registradores e retorno por `iretq`;
- objetos de processo com PID e dispatcher inicial de syscalls (`write`, `getpid`, `yield`);
- alocador de frames físicos e PML4 independente por processo;
- mapeamento de páginas de usuário com flags de proteção verificadas;
- ativacao real de CR3 com mapeamento supervisor do kernel preservado;
- loader ELF64 de segmentos `PT_LOAD` com validacao de limites, zero-fill e flags;
- binario `user/init.S` compilado separadamente, carregado e iniciado com CR3 e stack proprios;
- syscall `exit` com encerramento controlado no kernel;
- tabela pai/filho, reparenting ao sair, `fork` baseado em ELF e `waitpid`;
- teste de integracao criando filho ELF com PID/CR3 distintos e coletando status;
- libc user inicial com wrappers `write`, `getpid`, `fork` e `exit`;
- semantica de retorno do `fork`: PID no pai e zero no filho;
- execucao real do filho forkado com ELF, stack e CR3 proprios;
- estado `WAITING` e prototipo interno de `waitpid` bloqueante, ainda nao habilitado na syscall;
- thread do scheduler associada ao processo e entrada user iniciada por contexto proprio;
- preempcao protegida contra troca para contextos de thread ainda nao iniciados;
- filesystem persistente ATA com superbloco, bitmap de blocos, diretorios, arquivos e montagem;
- syscalls VFS e wrappers user para `mkdir`, `create`, `write_file` e `read_file`;
- programa user criando, escrevendo, lendo e exibindo um arquivo;
- driver ATA PIO LBA28 com leitura/escrita de setores em disco RAW do QEMU;
- framebuffer linear RGB32 compilavel, com desenho de pixels e retangulos via GRUB;
- fonte bitmap, compositor inicial, janelas de desktop e terminal grafico;
- driver de mouse PS/2 com eventos de movimento e cursor renderizado;
- driver Intel E1000 para a placa virtual do QEMU, com descoberta PCI, MAC e
	anéis de transmissão/recepção em modo polling;
- segmentos DPL 3, TSS e entrada real em ring 3 via `int 0x80`;
- headers libc/POSIX, sockets e framebuffer para o user space freestanding;
- runtime C++ freestanding inicial com hooks ABI e `new` baseado em `kmalloc`;
- ponte C++ do Dillo para framebuffer, frames, clipping e fila FIFO de eventos
	de teclado/mouse;
- fonte upstream do Dillo em `third_party/dillo`, mantida como base do port;
- linker script, simbolos de debug e alvos de build/teste.

O desktop atual nao e o Dillo: ele oferece apenas janelas e aplicativos
internos. O Dillo upstream usa C++, FLTK e um ambiente POSIX hospedado; seu
backend para OakOS ainda nao esta implementado. Duktape nao e um navegador e
permanece fora do build ate existir um consumidor real. O projeto nao declara
funcionalidades ainda nao implementadas.

## Requisitos

No Ubuntu, instale as ferramentas necessarias:

```sh
sudo apt install build-essential binutils grub-pc-bin grub-common xorriso qemu-system-x86 gdb
```

O build atual usa GCC/binutils nativos em modo freestanding; nao depende da
libc do sistema nem de um kernel hospedeiro durante a execucao.

## Compilar e executar

```sh
make              # compila o kernel e gera build/oak-os.iso
make limbo        # gera ISO e disco QCOW2 para o Limbo/QEMU Mobile
make test         # valida a estrutura ELF da imagem
make bearssl      # compila BearSSL para o ambiente freestanding
make dillo-port   # compila o Dillo upstream no host (nao gera binario OakOS)
make cxx-runtime-test # valida os hooks C++ freestanding sem libc
make dillo-platform-test # valida a ponte gráfica e eventos do Dillo
make run          # inicia QEMU com serial e disco RAW no terminal
make run-gui      # inicia QEMU com framebuffer e janela grafica
make run-vnc      # inicia QEMU e noVNC web na porta 6080 com cursor visivel
make debug        # inicia QEMU com E1000 aguardando GDB em localhost:1234
make clean
```

O alvo `dillo-port` requer FLTK 1.3 e as dependencias de imagem do host. Em um
ambiente sem instalacao global, e possivel passar um `fltk-config` extraido e
suas flags:

```sh
make dillo-port \
	FLTK_CONFIG="$PWD/build/host/usr/bin/x86_64-linux-gnu-fltk-config" \
	FLTK_CFLAGS="-I$PWD/build/host/usr/include" \
	FLTK_CXXFLAGS="-I$PWD/build/host/usr/include" \
	FLTK_LIBS="-L$PWD/build/host/usr/lib/x86_64-linux-gnu -lfltk -lX11"
```

O executavel host resultante fica em `build/dillo/src/dillo`. Ele nao pode ser
copiado para a ISO: o port nativo ainda precisa de um backend de FLTK/eventos,
da libc/POSIX e da pilha de rede do OakOS.

No Codespace, abra o endereco exibido pelo `make run-vnc`. Para escolher outra
porta HTTP, use `make run-vnc VNC_PORT=6081`; para evitar conflito na porta do
servidor VNC, use tambem `VNC_DISPLAY=2 VNC_SERVER_PORT=5902`.

No terminal grafico, use `run snake` ou `run snake.elf` para abrir o Snake em
tela cheia. Use WASD para mover, R para reiniciar e Ctrl+C para voltar ao shell.
`snake.elf` e o nome de lancamento registrado para o Snake grafico atual; o
launcher aceita apenas aplicativos registrados e um ELF arbitrario ainda nao e
executado pelo kernel.

Para depurar, em outro terminal use `gdb build/acorn.elf`, execute `target
remote :1234` e entao `continue`.

No terminal grafico, `net` informa se a placa E1000 foi encontrada. A camada
de rede atual implementa Ethernet/ARP, IPv4, DNS e um transporte TCP mínimo
com polling; retransmissão completa, congestionamento e protocolos superiores
ainda dependem das próximas etapas da pilha.

### Limbo/QEMU Mobile

Execute `make limbo`. Use `build/oakos-limbo.iso` como CD-ROM e, opcionalmente,
`build/oakos-limbo-data.qcow2` como segundo disco. Configure arquitetura x86_64,
chipset pc/i440FX, 512 MB de RAM e VGA padrao. O boot deve ser feito pela ISO;
o segundo disco guarda os dados persistentes do filesystem.

## Arquitetura

```text
boot/       entrada Assembly, Multiboot e transicao para long mode
kernel/     kernel Acorn e componentes de hardware
	include/  headers publicos internos do kernel
linker.ld  layout do ELF e ponto de entrada
grub.cfg   entrada da imagem ISO
build/      artefatos gerados, incluindo a imagem RAW do disco
```

## Roadmap

1. Boot, serial e VGA (implementado).
2. GDT, IDT e handler de excecoes (implementado; IRQs ainda desativadas).
3. Memory map, bitmap de frames e heap do kernel (implementado; virtual memory avancada pendente).
4. Timer, teclado e arquitetura de drivers (implementado; entrada traduzida validada no QEMU).
5. Scheduler, troca cooperativa e preempcao por timer (implementado; processos isolados pendentes).
6. Processos, ring 3 e syscalls (implementado; binarios separados pendentes).
7. Paginacao isolada, loader ELF e ciclo basico de processos (implementado; preempcao entre processos iniciados e wakeup bloqueante pendentes).
8. VFS persistente sobre ATA, syscalls de arquivos e ATA PIO (implementado).
9. Framebuffer, texto, entrada grafica, janelas, compositor e terminal inicial (implementado).
10. Runtime C++ freestanding e port do Dillo: completar allocator, adaptar
libc/POSIX, sockets, rede, eventos, FLTK e desenho ao framebuffer do OakOS.
11. Desktop, terminal grafico, ferramentas de programacao e estabilizacao.

Cada etapa deve passar por compilacao, imagem e teste no QEMU antes da
proxima.
# OakOS