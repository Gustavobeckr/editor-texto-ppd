# Editor de texto -- PPD

Este projeto consiste no desenvolvimento de um editor de texto colaborativo utilizando C++, com suporte a edição simultânea por múltiplos usuários, inspirado em ferramentas como Google Docs.

Bibliotecas utilizadas:
OpenMP: para execução paralela e otimização de operações locais, como salvamento automático e verificação de alterações.
MPI: para comunicação entre processos distribuídos, garantindo a sincronização das edições em tempo real entre os usuários.

Linguagem de programação utilizada: C++

## Lista de Tarefas

- ~~O programa deve possibilitar a utilização por múltiplos usuários simultaneamente;~~
- Deve assegurar que as alterações feitas fiquem imediatamente disponíveis para todos
os usuários;
- Deve assegurar a consistência;
- Deve possibilitar comunicação fechada entre dois usuários;
- Deve possibilitar gerar automaticamente um grande volume de dados para realizar
testes de desempenho