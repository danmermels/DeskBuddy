import json
import os

quotes_ptbr = {
  "first_sit": {
    "coach": [
      "Bom dia, {name}! O sono acabou. {detail} offline. Vamos fazer valer hoje!",
      "Acorde e brilhe, {name}! Te recebendo de volta após {detail}. Hora de construir sucesso!",
      "Saudações, {name}! Novo começo após {detail} de descanso. Firme suas metas hoje!",
      "Bom dia, {name}! {detail} de sono terminaram. Pronto para superar suas metas?",
      "Bem-vindo de volta, {name}! Vamos começar forte esta sessão após {detail} offline."
    ],
    "critic": [
      "Bom dia, {name}. Dormiu {detail}? Espero que planeje realmente trabalhar agora.",
      "Oh, {name}. Bem-vindo de volta após {detail}. Não comece a relaxar já.",
      "Acorde e brilhe, {name}. {detail} de ócio offline. Vamos ver se consegue focar.",
      "Saudações, {name}. De volta à mesa após {detail} ausente. Tente não disparatar.",
      "Bem-vindo, {name}! Você ficou ausente por {detail}. A fila de tarefas espera por você."
    ],
    "sweet": [
      "Bom dia, {name}! Dormiu bem durante seus {detail} ausente? Tenha um lindo dia.",
      "Acorde e brilhe, {name}! Um caloroso bem-vindo de volta após {detail}. Cuide-se hoje.",
      "Bom dia, {name}! Tão feliz em ver você após {detail} offline. Espero que sinta-se descansado!",
      "Bem-vindo de volta, {name}! Espero que seu descanso de {detail} foi pacífico e doce.",
      "Café pronto, {name}? Um caloroso bem-vindo de volta após {detail}. Vamos começar com calma."
    ],
    "friend": [
      "Bom dia, {name}! Feliz em te ver de volta após {detail} offline. Vamos divertir hoje.",
      "Bem-vindo de volta, {name}! Pronto para progredir após {detail} de sono?",
      "Olá, {name}! Espero que sinta-se renovado após {detail} de descanso. Hora da mágica.",
      "Acorde e brilhe, {name}! {detail} offline. Pronto para enfrentar o universo?",
      "Saudações, {name}! O café espera após seu descanso de {detail}. Acomode-se."
    ]
  },
  "late_hours": {
    "coach": [
      "Acordado a esta hora, {name}? {detail}. Qualquer que seja o motivo, faça valer.",
      "{name}, são {time} e o expediente não começa tão cedo. {detail}. Rápido e focado.",
      "Sessão noturna, {name}? {detail}. Tudo bem — mas mantenha curto.",
      "{name}, {detail}. Uma tarefa rápida e de volta ao descanso.",
      "A mesa fica aberta 24h, {name}. {detail}. Use as horas com sabedoria."
    ],
    "critic": [
      "{name}, {detail}. A fila não pode ser tão urgente a esta hora.",
      "Oh, {name}. {detail}. Espero que este não seja o plano para a noite toda.",
      "{name}, são {time}. {detail}. Até as tarefas estão dormindo.",
      "{name}, {detail}. Claro, mas lembre do que acontece quando você esgota.",
      "Verificando às {time}, {name}? {detail}. A esta hora, a fila não espera por ninguém."
    ],
    "sweet": [
      "Acordado a esta hora, {name}? {detail}. Por favor, não esqueça de descansar em breve.",
      "{name}, são {time} e aqui está você. {detail}. Cuide de si mesmo.",
      "A mesa está quieta às {time}, {name}. {detail}. Suave, rápido e depois durma.",
      "{name}, {detail}. Seja o que for, espero que valha a noite virada.",
      "Doce {name}, são {time}. {detail}. Termine com calma e descanse."
    ],
    "friend": [
      "{detail}, {name}. A mesa chamou e você atendeu a esta hora.",
      "São {time}, {name}. {detail}. Sua cama está te olhando feio.",
      "{name}, {detail}. Visitas à mesa neste horário estão virando hábito.",
      "Turno da noite, {name}? {detail}. Rápido, o café está cansado.",
      "{name}, o relógio marca {time}. {detail}. Até corujas precisam de um limite."
    ]
  },
  "welcome_back": {
    "coach": [
      "Pausa encerrada, {name}. {detail} foi sua janela de recuperação — agora trabalhe.",
      "De volta à cadeira, {name}. Aquela pausa de {detail} acabou. Foca agora.",
      "Hora de executar, {name}. Você esteve ausente por {detail}. Foco!",
      "Recarregado após {detail}, {name}? Vamos acelerar o passo.",
      "Bem-vindo de volta, {name}. Você tirou {detail} — agora conquiste esse progresso."
    ],
    "critic": [
      "Oh, você voltou, {name}. Aquela pausa de {detail} pareceu uma eternidade.",
      "Gentil da sua parte retornar, {name}. {detail} ausente não foi o bastante?",
      "De volta de onde quer que tenha vagado por {detail}, {name}. Vamos trabalhar.",
      "A mesa estava em paz sem você, {name}. A pausa de {detail} acabou.",
      "Acomode-se, {name}. Vamos ver se você foca mais de 5 minutos desta vez."
    ],
    "sweet": [
      "Bem-vindo de volta, {name}! Espero que tenha tido uma pausa de {detail} relaxante.",
      "Feliz em te ver de volta, {name}! Sua pausa de {detail} foi tranquila?",
      "Olá, querido {name}! Recarregado após {detail}? Não trabalhe demais.",
      "Espero que sua pausa de {detail} tenha sido revigorante, {name}. Acomode-se confortável.",
      "Bem-vindo de volta, {name}! Acomode-se, respire e foco com calma."
    ],
    "friend": [
      "Ei {name}, bem-vindo de volta! {detail} ausente. Achou algum café?",
      "Você voltou! A mesa sentiu sua falta durante esses {detail}, {name}.",
      "Certo {name}, pausa de {detail} encerrada. De volta à lida.",
      "Bem-vindo de volta, {name}. {detail} offline. Vamos fazer acontecer.",
      "De volta à ação, {name}. Vamos retomar de onde paramos."
    ]
  },
  "stretch": {
    "coach": [
      "Levante-se, {name}! Sua coluna está chorando. Mova essas pernas agora.",
      "Pausa, {name}! Fique em pé um minuto. Circulação é essencial.",
      "Postura de banana, {name}. Corrija e gire os ombros!",
      "Sentar é o novo fumar, {name}. Levante e estique-se!",
      "Levante-se, {name}! Sacuda. Energia alta gera resultado alto."
    ],
    "critic": [
      "Pretende fundir-se à cadeira, {name}? Levante-se.",
      "Sua postura é um desastre, {name}. Endireite ou levante.",
      "Ei {name}, olhe para algo além desta tela. Seus olhos derretem.",
      "Ainda sentado, {name}? Sua coluna vai virar um ponto de interrogação.",
      "Levante-se, {name}. Seus músculos estão atrofiando."
    ],
    "sweet": [
      "Hora de esticar, {name}! Seu corpo precisa de movimento.",
      "Gire os ombros, {name}. Inspire e relaxe.",
      "Hidrate-se, {name}! Vá beber água fresca agora.",
      "Pisque, {name}! Dê um descanso aos seus olhinhos.",
      "Respire fundo e estique, {name}. Você está sentado há tanto tempo."
    ],
    "friend": [
      "Ei {name}, levante e estique. Seu corpo agradece.",
      "Afaste-se da tela, {name}! Vá caminhar um pouco.",
      "Gire os punhos, {name}. Respire fundo rápido.",
      "Levante, {name}, e alcance o céu. Só um pequeno reset.",
      "Hora de esticar 1 minuto, {name}. Vamos sacudir."
    ]
  },
  "focus_end": {
    "coach": [
      "Sessão de foco completa! Ótima execução, {name}.",
      "Foco profundo alcançado, {name}! Você é uma máquina! Continue.",
      "Sessão de foco sólida, {name}. Agora eleve a meta na próxima.",
      "Você destruiu aquele bloco de foco, {name}! Mantenha o ritmo.",
      "Excelente sessão de foco, {name}. É assim que progredimos."
    ],
    "critic": [
      "Olhe só, {name}. Você realmente focou por {detail}.",
      "Sessão de foco completa. Não faça festa ainda, {name}.",
      "Você concentrou bem por {detail}, {name}. Me surpreenda.",
      "Sessão sólida, {name}. Esperemos que a próxima seja ainda melhor.",
      "Alvo de foco batido. Vamos ver se repete, {name}."
    ],
    "sweet": [
      "Trabalho profundo completo, {name}! Tão orgulhosa da sua concentração!",
      "Ótimo foco, {name}. Agora vá curtir uma pausa merecida.",
      "Você foi tão bem focando, {name}! Tire um descanso tranquilo agora.",
      "Trabalho brilhante mantendo o foco, {name}! Você merece um mimo.",
      "Sessão de foco acabou, {name}. Descanse a mente e os olhos agora."
    ],
    "friend": [
      "Chefe da produtividade! Faça uma reverência, {name}.",
      "Sessão de foco estelar, {name}! Tamo junto!",
      "Você ficou firme, {name}. Bom trabalho.",
      "Foco alcançado, {name}. Você ganhou um descanso.",
      "Você mandou naquele bloco de foco, {name}! Boa!"
    ]
  },
  "slacker": {
    "coach": [
      "Pontuação de foco baixa, {name}. Ajuste o foco e trave.",
      "Aquela lista não vai se terminar sozinha, {name}. Empurre!",
      "Seja lá o que faz, não é trabalho. Acalme e execute.",
      "O tempo passa, {name}. Pare de vagar e gere resultado.",
      "Foco baixo hoje, {name}. Vamos reverter isso agora."
    ],
    "critic": [
      "Procrastinando de novo, {name}? Estratégia ousada.",
      "Rolar feed conta como cardio agora? Novidade, {name}.",
      "Pontuação de foco: baixa. Desculpas: muitas. Conserta, {name}.",
      "Seu teclado está solitário, {name}. Dê atenção a ele.",
      "Se o esforço fosse opcional hoje, você mandaria, {name}."
    ],
    "sweet": [
      "Você parece distraído, {name}. Tudo bem?",
      "Sua pontuação de foco está tendo um dia ruim, {name}. Acomode com calma.",
      "Vamos tentar focar mais um pouco, {name}. Você consegue!",
      "Respire fundo, {name}, e vamos tentar voltar aos trilhos.",
      "Não deixe as distrações vencerem, {name}. Acredito em você."
    ],
    "friend": [
      "Esse é o ritmo que buscava hoje, {name}?",
      "Foco baixo. Potencial alto. Escolha, {name}.",
      "A rede chamou. Você atendeu. O trabalho espera, {name}.",
      "Você está à deriva, {name}. Volte à realidade.",
      "Menos navegação, mais ação. A matemática é simples, {name}."
    ]
  },
  "streak_beaten": {
    "coach": [
      "Novo recorde de sessão, {name}! Nível de foco máximo!",
      "Recorde de sequência quebrado, {name}! Persistência excepcional!",
      "Marco de sessão alcançado, {name}! Continue empurrando os limites.",
      "Recorde de sequência destruído, {name}! Você dita o ritmo.",
      "Novo recorde, {name}! Padrão alto estabelecido."
    ],
    "critic": [
      "Novo recorde, {name}. Sua cadeira deve estar muito orgulhosa.",
      "Sequência batida, {name}. Tente não criar raízes na cadeira.",
      "Sessão maratona, {name}. Espero que a academia esteja ativa.",
      "Nova sequência, {name}! Sentado como estátua. Impressionante.",
      "Recorde quebrado, {name}. Ainda assim, levante eventualmente."
    ],
    "sweet": [
      "Sequência batida, {name}! Você está pegando fogo hoje!",
      "Novo recorde pessoal de sessão, {name}! Tão orgulhosa!",
      "Incrível, {name}! Sessão mais longa do dia. Estique as pernas com calma.",
      "Você bateu seu recorde anterior de sessão, {name}! Maraviolhoso!",
      "Foco de elite, {name}! Só lembre de esticar as pernas."
    ],
    "friend": [
      "Campeão de sessão, {name}! Um recorde totalmente novo!",
      "Imparável, {name}! A cadeira é seu trono.",
      "Incrível, {name}! Nova sessão mais longa hoje.",
      "Sessão recorde, {name}! Você é um mago do foco.",
      "Novo recorde, {name}! Destruiu."
    ]
  },
  "lunch_reminder": {
    "coach": [
      "Hora do almoço, {name}! Reabasteça o corpo para a próxima metade.",
      "Pausa para nutrição, {name}! Vá almoçar e recarregar.",
      "Abasteça-se, {name}! A hora do almoço chegou. Mantenha a energia.",
      "Pausa de almoço, {name}! Afaste-se, coma e prepare para empurrar.",
      "Hora de comer, {name}! Corpo saudável sustenta mente afiada."
    ],
    "critic": [
      "O estômago ronca, {name}. Vá comer antes de cair.",
      "O almoço chama, {name}. Não ignore, você parece bravo de fome.",
      "Hora do almoço, {name}. Afaste da tela. O teclado espera.",
      "Dev faminto é dev irritado, {name}. Vá buscar comida.",
      "Hora de fechar o notebook e comer, {name}. Você já encarou demais."
    ],
    "sweet": [
      "Hora do almoço! Afaste da mesa e coma, querido {name}.",
      "Hora da comida, {name}! Não pule o almoço, é muito importante.",
      "Alimente seu cérebro, {name}! Hora de pegar um almoço gostoso.",
      "Vá almoçar, {name}! Bom apetite, cuide-se.",
      "Afaste e coma, {name}. Você precisa se nutrir."
    ],
    "friend": [
      "Hora do almoço, {name}! Vá comer algo.",
      "Pausa de almoço, {name}! Afaste e ache comida de verdade.",
      "Hora de recarregar com comida, {name}. Senta e relaxa.",
      "Pausa para comer, {name}! Você com certeza mereceu.",
      "Com fome, {name}? Pegue uma fatia de pizza ou algo assim."
    ]
  },
  "excessive_breaks": {
    "coach": [
      "Muitas pausas, {name}. Acomode e foco agora.",
      "Vamos tentar um bloco de trabalho maior desta vez, {name}. Acalme.",
      "Muitas pausas hoje. Acomode para um trabalho profundo, {name}.",
      "Sessão de foco chegando. Acomode e mantenha foco, {name}.",
      "Consistência é chave, {name}. Fique na mesa e execute."
    ],
    "critic": [
      "Você voltou de novo, {name}. São muitas pausas hoje.",
      "Entra e sai como dono de porta giratória, {name}. Foco.",
      "A cadeira está contando, {name}. Não está impressionada.",
      "Mais transições que resultados hoje. Acomode, {name}.",
      "De volta de novo. Acabe com isso desta vez e tente ficar."
    ],
    "sweet": [
      "Outro retorno. Vá com calma e acomode confortável, {name}.",
      "Bem-vindo de volta. Vamos mirar um bloco de foco tranquilo, {name}.",
      "Feliz em te ver de volta, {name}. Acomode e trabalhemos com calma.",
      "Pronto para uma sessão sem interrupção desta vez, querido {name}?",
      "Mais um retorno. Vamos trabalhar calma e focado, {name}."
    ],
    "friend": [
      "De volta de novo. A mesa é um pit stop hoje, {name}.",
      "Você subiu e desceu mais que uma ação, {name}.",
      "Sua razão pausa-trabalho está aventureira hoje, {name}.",
      "Na cadeira. De novo. Acomode desta vez, {name}.",
      "Bem-vindo de volta. Vamos trabalhar agora, {name}."
    ]
  },
  "goal_completed": {
    "coach": [
      "Meta diária completa! Você bateu as horas, {name}!",
      "Meta concluída, {name}! Você trabalhou as horas-alvo hoje.",
      "Meta de tempo na mesa batida! Missão cumprida. Mandou, {name}.",
      "Horas-alvo alcançadas! Ótimo esforço e disciplina hoje, {name}.",
      "Meta diária completa! Você bateu o alvo, {name}."
    ],
    "critic": [
      "Meta diária completa! Pode finalmente deslogar, {name}.",
      "Você bateu o alvo, {name}. Vá para casa antes que eu caia.",
      "Meta de mesa completa, {name}. Não trabalhe tanto amanhã.",
      "Horas-alvo completas. A mesa está livre de você agora, {name}.",
      "Meta concluída, {name}. Você realmente cumpriu as horas hoje."
    ],
    "sweet": [
      "Parabéns, {name}! Você alcançou sua meta diária de tempo na mesa!",
      "Meta alcançada, querido {name}! Você trabalhou as horas-alvo hoje.",
      "Meta diária completa! Orgulhosa do seu tempo hoje, {name}.",
      "Meta desbloqueada! Você bateu o alvo, {name}. Descanse.",
      "Meta diária cumprida, {name}! Vá relaxar e tenha uma boa noite."
    ],
    "friend": [
      "Meta de mesa completa, {name}! Ótima persistência!",
      "Alvo batido! Trabalho excelente hoje, {name}.",
      "Meta desbloqueada! Senta e comemora, {name}.",
      "Alvo de expediente alcançado! Bem feito, {name}.",
      "Meta diária alcançada! Você cumpriu o alvo diário, {name}."
    ]
  },
  "nagging": {
    "coach": [
      "Lembrete: {detail}, {name}. Vamos resolver essa tarefa!",
      "Pendência na mesa, {name}: {detail}. Vamos riscar isso agora.",
      "Foco na prioridade, {name}: {detail}. Ação supera intenção.",
      "{name}, {detail} ainda precisa de você. Mãos à obra!",
      "Atenção, {name}: {detail}. Resolva e avance."
    ],
    "critic": [
      "{detail} continua esperando, {name}. Vai encarar ou ignorar?",
      "Oh, veja só: {detail} ainda está pendente, {name}.",
      "{name}, {detail} não vai se resolver magicamente.",
      "Ainda enrolando com {detail}, {name}?",
      "Lembrete amigável: {detail} está mofando na sua lista, {name}."
    ],
    "sweet": [
      "Querido {name}, não esqueça de {detail} quando puder.",
      "Pequeno lembrete, {name}: {detail} espera por você com carinho.",
      "{name}, assim que der uma brecha, dê uma olhada em {detail}.",
      "Você consegue resolver {detail}, {name}! Um passo de cada vez.",
      "Lembrete suave, {name}: {detail} está quase lá!"
    ],
    "friend": [
      "Ei {name}, {detail} tá te chamando!",
      "Bora matar {detail}, {name}?",
      "Olha a pendência, {name}: {detail}. Vamos nessa!",
      "Não esquece de {detail}, {name}! Tamo junto.",
      "Hora de riscar {detail} da lista, {name}!"
    ]
  },
  "points": {
    "coach": [
      "55 minutos de foco, {name}! {detail}. Excelente ritmo, continue acumulando!",
      "Check-in de 55m, {name}! {detail}. Mantenha a disciplina em alta.",
      "Foco constante, {name}! {detail}. O progresso é visível.",
      "Mais 55m na conta, {name}! {detail}. Rumo ao topo!",
      "Ótimo ritmo, {name}! {detail}. Continue empilhando, o mês é seu."
    ],
    "critic": [
      "Uma hora sentado, {name}. {detail}. Esse número não se move sozinho.",
      "{name}, {detail}. Impressiona só se as tarefas saíram.",
      "55 minutos, {name}. {detail}. Não comemore; acumule mais.",
      "{name}, seu placar lê {detail}. Deixe vergonhoso — para o acúmulo.",
      "Você está em {detail}, {name}. Bom. Agora continue bom."
    ],
    "sweet": [
      "55 minutos de foco, {name} — bem feito. {detail}. Continue com calma.",
      "Você foi tão bem nesta hora, {name}. {detail}. Orgulhosa de você.",
      "Aqui seu check-in, {name}. {detail}. Cada tarefa add um brilho.",
      "{name}, você está construindo algo lindo. {detail}. Mais uma vitória pequena?",
      "Progresso doce, {name}. {detail}. Respire, depois continue fluindo."
    ],
    "friend": [
      "Hora dada, {name}! Placar: {detail}. Vamos subir.",
      "{name}, {detail}. Não mal — não mal mesmo. Outra rodada?",
      "55 minutos, {name}. {detail}. Vamos, mais uma tarefa para o mérito.",
      "{name}, o quadro de pontos diz {detail}. Vamos deixar vergonhoso-bom.",
      "De volta da hora, {name}: {detail}. Mantenha a sequência viva!"
    ]
  },
  "curation": {
    "coach": [
      "Notei algo, {name}: {detail}. Use. É um padrão que vale agir.",
      "{name}, {detail}. Consciência é a primeira ferramenta — decida o que fazer.",
      "Veja o que percebo, {name}: {detail}. Pequeno dado, grande sinal.",
      "Observação para você, {name}: {detail}. Padrões viram hábitos. Escolha bem.",
      "Notando isso, {name}: {detail}. Os melhores rastreiam os detalhes."
    ],
    "critic": [
      "Olhe isso, {name}: {detail}. A evidência não mente.",
      "{name}, {detail}. Diria 'interessante' mas você sabe o que significa.",
      "Alerta de padrão: {detail}. {name}, você não é sutil.",
      "Pois pois, {name}: {detail}. Os dados têm opiniões sobre você hoje.",
      "{name}, {detail}. Sou só o mensageiro — e o mensageiro julga."
    ],
    "sweet": [
      "Oi {name}! Só notando: {detail}. Espero que esteja bem.",
      "{name}, notei algo hoje: {detail}. Cuide de si mesmo.",
      "Uma pequena observação, {name}: {detail}. Sem pressão, só consciência.",
      "Doce amigo {name}, {detail}. Só quis que saiba que presto atenção.",
      "{name}! {detail}. Pequeno, mas vale menção gentil."
    ],
    "friend": [
      "Ei {name}, fato curioso: {detail}. A mesa vê tudo.",
      "{name}, confira: {detail}. Nem bom nem mal — só dado com bigode.",
      "Sua mesa sussurrou: {detail}. {name}, está ficando filosófica.",
      "{name}! {detail}. É sua mesa falando. Agora ela tem opiniões.",
      "Cara, {detail}. {name}, não estou dizendo nada, só DIGO."
    ]
  }
}

quotes_en = {
  "first_sit": {
    "coach": [
      "Morning, {name}! Sleep is over. {detail} offline. Let's make today count!",
      "Rise and shine, {name}! Welcoming you back after {detail}. Time to build success!",
      "Greetings, {name}! Fresh start after {detail} rest. Lock in your goals today!",
      "Good morning, {name}! {detail} sleep is done. Ready to crush your targets?",
      "Welcome back, {name}! Let's launch this session strong after {detail} offline."
    ],
    "critic": [
      "Morning, {name}. Slept for {detail}? Hope you're planning to actually work now.",
      "Oh, {name}. Welcome back after {detail}. Don't start slackin' already.",
      "Rise and shine, {name}. {detail} of offline idling. Let's see if you can focus.",
      "Greetings, {name}. Back at the desk after {detail} away. Try not to drift off.",
      "Welcome, {name}! You were away for {detail}. The backlog is waiting for you."
    ],
    "sweet": [
      "Good morning, {name}! Slept well during your {detail} away? Have a beautiful day.",
      "Rise and shine, {name}! A warm welcome back after {detail}. Take care today.",
      "Morning, {name}! So glad to see you after {detail} offline. Hope you feel rested!",
      "Welcome back, {name}! Hope your {detail} rest was peaceful and sweet.",
      "Coffee ready, {name}? A warm welcome back after {detail}. Let's have a gentle start."
    ],
    "friend": [
      "Morning, {name}! Glad you are back after {detail} offline. Let's make today fun.",
      "Welcome back, {name}! Ready to make progress after {detail} sleep?",
      "Hello, {name}! Hope you feel refreshed after {detail} rest. Time for magic.",
      "Rise and shine, {name}! {detail} offline. Ready to tackle the universe?",
      "Greetings, {name}! Coffee is waiting after your {detail} rest. Settle in."
    ]
  },
  "late_hours": {
    "coach": [
      "Up at this hour, {name}? {detail}. Whatever pulled you in, make it count.",
      "{name}, it's {time} and the workday doesn't start for a while. {detail}. Brief and focused.",
      "Late-night desk session, {name}? {detail}. Fine — but keep it short.",
      "{name}, {detail}. One quick task and back to rest.",
      "The desk is open round the clock, {name}. {detail}. Use the hours wisely."
    ],
    "critic": [
      "{name}, {detail}. The backlog can't be that urgent at this hour.",
      "Oh, {name}. {detail}. Hope this isn't the plan for the whole night.",
      "{name}, it's {time}. {detail}. Even the tasks are asleep.",
      "{name}, {detail}. Sure, but remember what happens when you burn out.",
      "Checking in at {time}, {name}? {detail}. At this hour, the backlog waits for no one."
    ],
    "sweet": [
      "Awake at this hour, {name}? {detail}. Please don't forget to rest soon.",
      "{name}, it's {time} and here you are. {detail}. Take care of yourself.",
      "The desk is quiet at {time}, {name}. {detail}. Gentle, quick, then sleep.",
      "{name}, {detail}. Whatever it is, I hope it's worth the late night.",
      "Sweet {name}, it's {time}. {detail}. Finish up gently and rest."
    ],
    "friend": [
      "{detail}, {name}. The desk called, and you answered at this hour.",
      "It's {time}, {name}. {detail}. Your bed is giving you a look.",
      "{name}, {detail}. Same-time desk visits are starting to feel like a habit.",
      "Late shift, {name}? {detail}. Make it quick, the coffee's tired.",
      "{name}, the clock says {time}. {detail}. Even night owls need a cutoff."
    ]
  },
  "welcome_back": {
    "coach": [
      "Break over, {name}. {detail} was your recovery window — now let's work.",
      "Back in the seat, {name}. That {detail} break is done. Lock in now.",
      "Time to execute, {name}. You've been away for {detail}. Focus!",
      "Recharged after {detail}, {name}? Let's pick up the pace.",
      "Welcome back, {name}. You took {detail} off — now earn that progress."
    ],
    "critic": [
      "Oh, you're back, {name}. That {detail} break felt like an eternity.",
      "Nice of you to return, {name}. Was {detail} away not enough?",
      "Back from wherever you wandered for {detail}, {name}. Let's get to it.",
      "The desk was peaceful without you, {name}. {detail} break is over.",
      "Settle in, {name}. Let's see if you can focus for more than 5 minutes this time."
    ],
    "sweet": [
      "Welcome back, {name}! Hope you had a nice, relaxing {detail} break.",
      "Glad you're back, {name}! Was your {detail} break peaceful?",
      "Hello, dear {name}! Recharged after {detail}? Don't work too hard.",
      "Hope your {detail} break was refreshing, {name}. Settle in comfortably.",
      "Welcome back, {name}! Settle in, take a breath, and focus gently."
    ],
    "friend": [
      "Hey {name}, welcome back! {detail} away. Did you find any coffee?",
      "You returned! The desk missed you during those {detail}, {name}.",
      "Alright {name}, {detail} break done. Back to the grindstone.",
      "Welcome back, {name}. {detail} offline. Let's make things happen.",
      "Back in action, {name}. Let's pick up where we left off."
    ]
  },
  "stretch": {
    "coach": [
      "Stand up, {name}! Your spine is crying. Move those legs now.",
      "Time out, {name}! Stand up for a minute. Circulation is key.",
      "Postured like a banana, {name}. Fix it and roll your shoulders!",
      "Sitting is the new smoking, {name}. Stand up and stretch!",
      "Stand up, {name}! Shake it out. High energy breeds high output."
    ],
    "critic": [
      "Are you planning to fuse with that chair, {name}? Stand up.",
      "Your posture is a disaster, {name}. Sit up or get up.",
      "Hey {name}, look at something besides this screen. Your eyes are melting.",
      "Still sitting, {name}? Your spine is going to look like a question mark.",
      "Stand up, {name}. Your muscles are starting to atrophy."
    ],
    "sweet": [
      "Time to stretch, {name}! Your body needs a little movement.",
      "Roll your shoulders, {name}. Breathe in and relax.",
      "Hydrate, {name}! Go get some fresh water right now.",
      "Blink, {name}! Give your sweet eyes a little rest.",
      "Breathe deeply and stretch, {name}. You've been sitting so long."
    ],
    "friend": [
      "Hey {name}, stand up and stretch. Your body will thank you.",
      "Step away from the screen, {name}! Go walk around a bit.",
      "Roll your wrists, {name}. Take a quick breath.",
      "Stand up, {name}, and reach for the sky. Just a little reset.",
      "Time for a 1-minute stretch, {name}. Let's shake it out."
    ]
  },
  "focus_end": {
    "coach": [
      "Focus session complete! Great execution, {name}.",
      "Deep focus achieved, {name}! You're a beast! Keep it going.",
      "Solid focus session, {name}. Now raise the bar for the next one.",
      "You crushed that focus block, {name}! Keep the momentum.",
      "Excellent focus session, {name}. That's how we make progress."
    ],
    "critic": [
      "Look at that, {name}. You actually stayed focused for {detail}.",
      "Focus session complete. Don't throw a party just yet, {name}.",
      "You concentrated well for {detail}, {name}. Color me surprised.",
      "Solid focus session, {name}. Hopefully the next one is even better.",
      "Focus target hit. Let's see if you can repeat it, {name}."
    ],
    "sweet": [
      "Deep work complete, {name}! So proud of your concentration!",
      "Great focus, {name}. Now go enjoy a well-deserved break.",
      "You did so well focusing, {name}! Take a peaceful rest now.",
      "Brilliant work staying focused, {name}! You deserve a treat.",
      "Focus session ended, {name}. Rest your mind and eyes now."
    ],
    "friend": [
      "Productivity boss! Take a bow, {name}.",
      "Stellar focus session, {name}! High five!",
      "You stayed locked in, {name}. Great job.",
      "Focus achieved, {name}. You definitely earned a rest.",
      "You ruled that focus block, {name}! Good stuff."
    ]
  },
  "slacker": {
    "coach": [
      "Focus score is low, {name}. Fix your focus and lock in.",
      "That task list isn't going to finish itself, {name}. Push!",
      "Whatever you're doing, it's not work. Settle down and execute.",
      "Time is moving, {name}. Stop idling and get results.",
      "Low focus today, {name}. Let's turn this around right now."
    ],
    "critic": [
      "Procrastinating again, {name}? Bold strategy.",
      "Scrolling counts as cardio now? News to me, {name}.",
      "Focus score: low. Excuses: plenty. Fix it, {name}.",
      "Your keyboard is lonely, {name}. Give it some attention.",
      "If effort were optional today, you'd be crushing it, {name}."
    ],
    "sweet": [
      "You seem a little distracted, {name}. Is everything okay?",
      "Your focus score is having a rough day, {name}. Settle in gently.",
      "Let's try to focus a bit more, {name}. You can do it!",
      "Take a deep breath, {name}, and let's try to get back on track.",
      "Don't let the distractions win, {name}. I believe in you."
    ],
    "friend": [
      "Is this the pace you were aiming for today, {name}?",
      "Low focus. High potential. Make a choice, {name}.",
      "Social media called. You answered. Work is still waiting, {name}.",
      "You're drifting, {name}. Drift back to reality.",
      "Less browsing, more doing. The math is simple, {name}."
    ]
  },
  "streak_beaten": {
    "coach": [
      "New sitting record, {name}! Focus level maximum!",
      "Streak record broken, {name}! Outstanding persistence!",
      "Sitting milestone reached, {name}! Keep pushing the limits.",
      "Streak record smashed, {name}! You're setting the pace.",
      "New record, {name}! High standard established."
    ],
    "critic": [
      "New record, {name}. Your chair must be very proud.",
      "Streak beaten, {name}. Try not to grow roots in that seat.",
      "Marathon sitting, {name}. Hope your gym membership is active.",
      "New streak, {name}! Sitting like a statue. Very impressive.",
      "Record broken, {name}. Still, let's try to get up eventually."
    ],
    "sweet": [
      "Streak beaten, {name}! You are on fire today!",
      "New personal best sitting streak, {name}! So proud!",
      "Amazing, {name}! Longest sit of the day. Take a gentle stretch now.",
      "You beat your previous sitting record, {name}! Wonderful!",
      "Elite focus, {name}! Just remember to stretch your legs."
    ],
    "friend": [
      "Sitting champion, {name}! A brand new record!",
      "Unstoppable, {name}! The chair is your throne.",
      "Incredible, {name}! New longest sit today.",
      "Record sitting session, {name}! You're a focus wizard.",
      "New record, {name}! Smashed it."
    ]
  },
  "lunch_reminder": {
    "coach": [
      "Time for lunch, {name}! Refuel your body for the next half.",
      "Nutrition break, {name}! Go grab lunch and recharge.",
      "Fuel up, {name}! Lunch hour is here. Keep your energy high.",
      "Lunch break, {name}! Step away, eat, and get ready to push.",
      "Time to eat, {name}! Healthy body supports a sharp mind."
    ],
    "critic": [
      "Stomach is growling, {name}. Go eat before you collapse.",
      "Lunch is calling, {name}. Don't ignore it, you look hangry.",
      "Time for lunch, {name}. Step away from the screen. Keyboard will wait.",
      "A hungry developer is a cranky developer, {name}. Go get food.",
      "Time to shut the laptop and eat, {name}. You've stared enough."
    ],
    "sweet": [
      "Lunch time! Step away from the desk and eat, dear {name}.",
      "Food time, {name}! Don't skip lunch, it's very important.",
      "Feed your brain, {name}! Time to grab a delicious lunch.",
      "Go get some lunch, {name}! Bon appetit, take care.",
      "Step away and eat, {name}. You need to nourish yourself."
    ],
    "friend": [
      "Time for lunch, {name}! Go grab a bite to eat.",
      "Lunch break, {name}! Step away and find some actual food.",
      "Time to recharge with some food, {name}. Settle down.",
      "Break for food, {name}! You've definitely earned it.",
      "Hungry, {name}? Grab a slice of pizza or something."
    ]
  },
  "excessive_breaks": {
    "coach": [
      "Break count is high, {name}. Settle in and focus now.",
      "Let's try a longer work block this time, {name}. Settle down.",
      "High break count today. Settle in for some deep work, {name}.",
      "Focus session incoming. Settle in and stay focused, {name}.",
      "Consistency is key, {name}. Stay at the desk and execute."
    ],
    "critic": [
      "You're back again, {name}. That's a lot of breaks today.",
      "In and out like you own a revolving door, {name}. Focus.",
      "The chair's keeping count, {name}. It is not impressed.",
      "More transitions than results today. Settle in, {name}.",
      "Back again. Settle down this time and try to stay put."
    ],
    "sweet": [
      "Another return. Take it easy and settle in comfortably, {name}.",
      "Welcome back. Let's aim for a nice, quiet focus block, {name}.",
      "Glad you're back, {name}. Settle in and let's work gently.",
      "Ready for an uninterrupted session this time, dear {name}?",
      "One more return. Let's work calmly and focused, {name}."
    ],
    "friend": [
      "Back again. The desk is a pit stop today, {name}.",
      "You've been up and down more than a stock ticker, {name}.",
      "Your break-to-work ratio is adventurous today, {name}.",
      "In the chair. Again. Settle in this time, {name}.",
      "Welcome back. Let's get some work done now, {name}."
    ]
  },
  "goal_completed": {
    "coach": [
      "Daily target complete! You hit your workday hours, {name}!",
      "Goal completed, {name}! You've worked your target hours today.",
      "Desk time goal hit! Mission complete. Smashed it, {name}.",
      "Target hours achieved! Great effort and discipline today, {name}.",
      "Daily goal complete! You hit the workday target, {name}."
    ],
    "critic": [
      "Daily target complete! You can finally log off now, {name}.",
      "You hit the workday target, {name}. Go home before I collapse.",
      "Desk goal complete, {name}. Don't work too hard tomorrow.",
      "Target hours complete. The desk is free of you now, {name}.",
      "Goal completed, {name}. You actually did your hours today."
    ],
    "sweet": [
      "Congratulations, {name}! You reached your daily desk time goal!",
      "Goal achieved, dear {name}! You've worked your target hours today.",
      "Daily target complete! Proud of your desk time today, {name}.",
      "Goal unlocked! You hit the workday target, {name}. Rest up.",
      "Daily goal is fully met, {name}! Go relax and have a nice evening."
    ],
    "friend": [
      "Desk goal complete, {name}! Great persistence!",
      "Target hit! Awesome job working today, {name}.",
      "Goal unlocked! Settle down and celebrate, {name}.",
      "Workday target achieved! Well done, {name}.",
      "Daily goal reached! You checked off your daily desk target, {name}."
    ]
  },
  "nagging": {
    "coach": [
      "Reminder: {detail}, {name}. Let's get this task done!",
      "Backlog item, {name}: {detail}. Cross it off now.",
      "Focus on priority, {name}: {detail}. Action beats intent.",
      "{name}, {detail} still needs you. Let's move!",
      "Attention, {name}: {detail}. Resolve and move on."
    ],
    "critic": [
      "{detail} is still waiting, {name}. Face it or ignore it?",
      "Oh look: {detail} is still pending, {name}.",
      "{name}, {detail} won't magically solve itself.",
      "Still stalling on {detail}, {name}?",
      "Friendly reminder: {detail} is rotting on your list, {name}."
    ],
    "sweet": [
      "Dear {name}, don't forget {detail} when you can.",
      "Little reminder, {name}: {detail} waits for you warmly.",
      "{name}, whenever you get a break, check {detail}.",
      "You can solve {detail}, {name}! One step at a time.",
      "Gentle reminder, {name}: {detail} is almost there!"
    ],
    "friend": [
      "Hey {name}, {detail} is calling you!",
      "Let's crush {detail}, {name}?",
      "Check the backlog, {name}: {detail}. Let's go!",
      "Don't forget {detail}, {name}! We're in this together.",
      "Time to cross {detail} off the list, {name}!"
    ]
  },
  "points": {
    "coach": [
      "55 minutes of focus, {name}! {detail}. Great pace, keep building!",
      "55m check-in, {name}! {detail}. Keep discipline high.",
      "Steady focus, {name}! {detail}. Progress is clear.",
      "Another 55m in the bank, {name}! {detail}. Heading to the top!",
      "Great pace, {name}! {detail}. Keep stacking, the month is yours."
    ],
    "critic": [
      "An hour of sitting, {name}. {detail}. That number won't move itself.",
      "{name}, {detail}. Impressive only if the tasks actually got done.",
      "55 minutes, {name}. {detail}. Don't celebrate; bank some more.",
      "{name}, your scoreboard reads {detail}. Make it embarrassing — for the backlog.",
      "You're at {detail}, {name}. Good. Now stay good."
    ],
    "sweet": [
      "55 minutes of focus, {name} — well done. {detail}. Keep going gently.",
      "You've done so well this hour, {name}. {detail}. Proud of you.",
      "Here's your little check-in, {name}. {detail}. Every task adds a sparkle.",
      "{name}, you're building something lovely. {detail}. One more small win?",
      "Sweet progress, {name}. {detail}. Take a breath, then keep flowing."
    ],
    "friend": [
      "Hour's up, {name}! Score check: {detail}. Let's run it up.",
      "{name}, {detail}. Not bad — not bad at all. Another round?",
      "55 minutes, {name}. {detail}. C'mon, one more task for the bragging rights.",
      "{name}, the points board says {detail}. Let's make it embarrassing-good.",
      "Back from the hour mark, {name}: {detail}. Keep the streak alive!"
    ]
  },
  "curation": {
    "coach": [
      "Spotted something, {name}: {detail}. Use it. That's a pattern worth acting on.",
      "{name}, {detail}. Awareness is the first tool — now decide what to do with it.",
      "Here's what I'm seeing, {name}: {detail}. Small data point, big signal.",
      "Observation for you, {name}: {detail}. Patterns become habits. Choose wisely.",
      "Noticing this, {name}: {detail}. The best performers track the little things."
    ],
    "critic": [
      "Look at this, {name}: {detail}. The evidence doesn't lie.",
      "{name}, {detail}. I'd say 'interesting' but you know what it really means.",
      "Pattern alert: {detail}. {name}, you're not subtle.",
      "Well well, {name}: {detail}. The data has opinions about you today.",
      "{name}, {detail}. I'm just the messenger — and the messenger is judging."
    ],
    "sweet": [
      "Hi {name}! Just noticing: {detail}. Hope you're doing okay.",
      "{name}, I noticed something today: {detail}. Take care of yourself.",
      "A little observation, {name}: {detail}. No pressure, just awareness.",
      "Sweet friend {name}, {detail}. Just wanted you to know I'm paying attention.",
      "{name}! {detail}. Small thing, but worth a gentle mention."
    ],
    "friend": [
      "Hey {name}, fun fact: {detail}. The desk sees all.",
      "{name}, check it: {detail}. Not good, not bad — just data with a mustache.",
      "Your desk just whispered: {detail}. {name}, it's getting philosophical.",
      "{name}! {detail}. This is your desk talking. It has opinions now.",
      "Dude, {detail}. {name}, I'm not saying anything, I'm just SAYING."
    ]
  }
}

target_ptbr = os.path.join(os.getcwd(), 'data', 'fallbackquotes_ptbr.json')
with open(target_ptbr, 'w', encoding='utf-8') as f:
  json.dump(quotes_ptbr, f, ensure_ascii=False, indent=2)

target_en = os.path.join(os.getcwd(), 'data', 'fallbackquotes_en.json')
with open(target_en, 'w', encoding='utf-8') as f:
  json.dump(quotes_en, f, ensure_ascii=False, indent=2)

# Also generate fallbackquotes.json as default
target_default = os.path.join(os.getcwd(), 'data', 'fallbackquotes.json')
with open(target_default, 'w', encoding='utf-8') as f:
  json.dump(quotes_ptbr, f, ensure_ascii=False, indent=2)

print("Successfully generated fallbackquotes_ptbr.json, fallbackquotes_en.json, and fallbackquotes.json")
