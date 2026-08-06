#ifndef BEHAVIOUR_PTBR_H
#define BEHAVIOUR_PTBR_H

#include <Arduino.h>

// --- Tipos de Evento ---
#define EVENT_FIRST_SIT     0
#define EVENT_WELCOME_BACK  1
#define EVENT_STRETCH       2
#define EVENT_FOCUS_END     3
#define EVENT_SLACKER       4
#define EVENT_STREAK_BEATEN 5
#define EVENT_LUNCH_REMINDER 6
#define EVENT_EXCESSIVE_BREAKS 8
#define EVENT_GOAL_COMPLETED   9
#define EVENT_JOURNAL          10
#define EVENT_NAGGING          11
#define EVENT_TASK_DUE         12
#define EVENT_PAGE             13
#define EVENT_LATEHOURS_SIT    14
#define EVENT_POINTS           15
#define EVENT_CURATION         16

// --- Frases locais de fallback/eco (20 por categoria) ---

static const char* localFirstSit[4][5] = {
  { // Coach
    "Bom dia, {name}! O sono acabou. {detail} offline. Vamos fazer valer hoje!",
    "Acorde e brilhe, {name}! Te recebendo de volta após {detail}. Hora de construir sucesso!",
    "Saudações, {name}! Novo começo após {detail} de descanso. Firme suas metas hoje!",
    "Bom dia, {name}! {detail} de sono terminaram. Pronto para superar suas metas?",
    "Bem-vindo de volta, {name}! Vamos começar forte esta sessão após {detail} offline."
  },
  { // Critic
    "Bom dia, {name}. Dormiu {detail}? Espero que planeje realmente trabalhar agora.",
    "Oh, {name}. Bem-vindo de volta após {detail}. Não comece a relaxar já.",
    "Acorde e brilhe, {name}. {detail} de ócio offline. Vamos ver se consegue focar.",
    "Saudações, {name}. De volta à mesa após {detail} ausente. Tente não disparatar.",
    "Bem-vindo, {name}! Você ficou ausente por {detail}. A fila de tarefas espera por você."
  },
  { // Sweet
    "Bom dia, {name}! Dormiu bem durante seus {detail} ausente? Tenha um lindo dia.",
    "Acorde e brilhe, {name}! Um caloroso bem-vindo de volta após {detail}. Cuide-se hoje.",
    "Bom dia, {name}! Tão feliz em ver você após {detail} offline. Espero que sinta-se descansado!",
    "Bem-vindo de volta, {name}! Espero que seu descanso de {detail} foi pacífico e doce.",
    "Café pronto, {name}? Um caloroso bem-vindo de volta após {detail}. Vamos começar com calma."
  },
  { // Friend
    "Bom dia, {name}! Feliz em te ver de volta após {detail} offline. Vamos divertir hoje.",
    "Bem-vindo de volta, {name}! Pronto para progredir após {detail} de sono?",
    "Olá, {name}! Espero que sinta-se renovado após {detail} de descanso. Hora da mágica.",
    "Acorde e brilhe, {name}! {detail} offline. Pronto para enfrentar o universo?",
    "Saudações, {name}! O café espera após seu descanso de {detail}. Acomode-se."
  }
};

static const char* localLateHours[4][5] = {
  { // Coach
    "Acordado a esta hora, {name}? {detail}. Qualquer que seja o motivo, faça valer.",
    "{name}, são {time} e o expediente não começa tão cedo. {detail}. Rápido e focado.",
    "Sessão noturna, {name}? {detail}. Tudo bem — mas mantenha curto.",
    "{name}, {detail}. Uma tarefa rápida e de volta ao descanso.",
    "A mesa fica aberta 24h, {name}. {detail}. Use as horas com sabedoria."
  },
  { // Critic
    "{name}, {detail}. A fila não pode ser tão urgente a esta hora.",
    "Oh, {name}. {detail}. Espero que este não seja o plano para a noite toda.",
    "{name}, são {time}. {detail}. Até as tarefas estão dormindo.",
    "{name}, {detail}. Claro, mas lembre do que acontece quando você esgota.",
    "Verificando às {time}, {name}? {detail}. A esta hora, a fila não espera por ninguém."
  },
  { // Sweet
    "Acordado a esta hora, {name}? {detail}. Por favor, não esqueça de descansar em breve.",
    "{name}, são {time} e aqui está você. {detail}. Cuide de si mesmo.",
    "A mesa está quieta às {time}, {name}. {detail}. Suave, rápido e depois durma.",
    "{name}, {detail}. Seja o que for, espero que valha a noite virada.",
    "Doce {name}, são {time}. {detail}. Termine com calma e descanse."
  },
  { // Friend
    "{detail}, {name}. A mesa chamou e você atendeu a esta hora.",
    "São {time}, {name}. {detail}. Sua cama está te olhando feio.",
    "{name}, {detail}. Visitas à mesa neste horário estão virando hábito.",
    "Turno da noite, {name}? {detail}. Rápido, o café está cansado.",
    "{name}, o relógio marca {time}. {detail}. Até corujas precisam de um limite."
  }
};

static const char* localWelcomeBack[4][5] = {
  { // Coach
    "Pausa encerrada, {name}. {detail} foi sua janela de recuperação — agora trabalhe.",
    "De volta à cadeira, {name}. Aquela pausa de {detail} acabou. Foca agora.",
    "Hora de executar, {name}. Você esteve ausente por {detail}. Foco!",
    "Recarregado após {detail}, {name}? Vamos acelerar o passo.",
    "Bem-vindo de volta, {name}. Você tirou {detail} — agora conquiste esse progresso."
  },
  { // Critic
    "Oh, você voltou, {name}. Aquela pausa de {detail} pareceu uma eternidade.",
    "Gentil da sua parte retornar, {name}. {detail} ausente não foi o bastante?",
    "De volta de onde quer que tenha vagado por {detail}, {name}. Vamos trabalhar.",
    "A mesa estava em paz sem você, {name}. A pausa de {detail} acabou.",
    "Acomode-se, {name}. Vamos ver se você foca mais de 5 minutos desta vez."
  },
  { // Sweet
    "Bem-vindo de volta, {name}! Espero que tenha tido uma pausa de {detail} relaxante.",
    "Feliz em te ver de volta, {name}! Sua pausa de {detail} foi tranquila?",
    "Olá, querido {name}! Recarregado após {detail}? Não trabalhe demais.",
    "Espero que sua pausa de {detail} tenha sido revigorante, {name}. Acomode-se confortável.",
    "Bem-vindo de volta, {name}! Acomode-se, respire e foco com calma."
  },
  { // Friend
    "Ei {name}, bem-vindo de volta! {detail} ausente. Achou algum café?",
    "Você voltou! A mesa sentiu sua falta durante esses {detail}, {name}.",
    "Certo {name}, pausa de {detail} encerrada. De volta à lida.",
    "Bem-vindo de volta, {name}. {detail} offline. Vamos fazer acontecer.",
    "De volta à ação, {name}. Vamos retomar de onde paramos."
  }
};

static const char* localStretch[4][5] = {
  { // Coach
    "Levante-se, {name}! Sua coluna está chorando. Mova essas pernas agora.",
    "Pausa, {name}! Fique em pé um minuto. Circulação é essencial.",
    "Postura de banana, {name}. Corrija e gire os ombros!",
    "Sentar é o novo fumar, {name}. Levante e estique-se!",
    "Levante-se, {name}! Sacuda. Energia alta gera resultado alto."
  },
  { // Critic
    "Pretende fundir-se à cadeira, {name}? Levante-se.",
    "Sua postura é um desastre, {name}. Endireite ou levante.",
    "Ei {name}, olhe para algo além desta tela. Seus olhos derretem.",
    "Ainda sentado, {name}? Sua coluna vai virar um ponto de interrogação.",
    "Levante-se, {name}. Seus músculos estão atrofiando."
  },
  { // Sweet
    "Hora de esticar, {name}! Seu corpo precisa de movimento.",
    "Gire os ombros, {name}. Inspire e relaxe.",
    "Hidrate-se, {name}! Vá beber água fresca agora.",
    "Pisque, {name}! Dê um descanso aos seus olhinhos.",
    "Respire fundo e estique, {name}. Você está sentado há tanto tempo."
  },
  { // Friend
    "Ei {name}, levante e estique. Seu corpo agradece.",
    "Afaste-se da tela, {name}! Vá caminhar um pouco.",
    "Gire os punhos, {name}. Respire fundo rápido.",
    "Levante, {name}, e alcance o céu. Só um pequeno reset.",
    "Hora de esticar 1 minuto, {name}. Vamos sacudir."
  }
};

static const char* localFocus[4][5] = {
  { // Coach
    "Sessão de foco completa! Ótima execução, {name}.",
    "Foco profundo alcançado, {name}! Você é uma máquina! Continue.",
    "Sessão de foco sólida, {name}. Agora eleve a meta na próxima.",
    "Você destruiu aquele bloco de foco, {name}! Mantenha o ritmo.",
    "Excelente sessão de foco, {name}. É assim que progredimos."
  },
  { // Critic
    "Olhe só, {name}. Você realmente focou por {detail}.",
    "Sessão de foco completa. Não faça festa ainda, {name}.",
    "Você concentrou bem por {detail}, {name}. Me surpreenda.",
    "Sessão sólida, {name}. Esperemos que a próxima seja ainda melhor.",
    "Alvo de foco batido. Vamos ver se repete, {name}."
  },
  { // Sweet
    "Trabalho profundo completo, {name}! Tão orgulhosa da sua concentração!",
    "Ótimo foco, {name}. Agora vá curtir uma pausa merecida.",
    "Você foi tão bem focando, {name}! Tire um descanso tranquilo agora.",
    "Trabalho brilhante mantendo o foco, {name}! Você merece um mimo.",
    "Sessão de foco acabou, {name}. Descanse a mente e os olhos agora."
  },
  { // Friend
    "Chefe da produtividade! Faça uma reverência, {name}.",
    "Sessão de foco estelar, {name}! Tamo junto!",
    "Você ficou firme, {name}. Bom trabalho.",
    "Foco alcançado, {name}. Você ganhou um descanso.",
    "Você mandou naquele bloco de foco, {name}! Boa!"
  }
};

static const char* localSlacker[4][5] = {
  { // Coach
    "Pontuação de foco baixa, {name}. Ajuste o foco e trave.",
    "Aquela lista não vai se terminar sozinha, {name}. Empurre!",
    "Seja lá o que faz, não é trabalho. Acalme e execute.",
    "O tempo passa, {name}. Pare de vagar e gere resultado.",
    "Foco baixo hoje, {name}. Vamos reverter isso agora."
  },
  { // Critic
    "Procrastinando de novo, {name}? Estratégia ousada.",
    "Rolar feed conta como cardio agora? Novidade, {name}.",
    "Pontuação de foco: baixa. Desculpas: muitas. Conserta, {name}.",
    "Seu teclado está solitário, {name}. Dê atenção a ele.",
    "Se o esforço fosse opcional hoje, você mandaria, {name}."
  },
  { // Sweet
    "Você parece distraído, {name}. Tudo bem?",
    "Sua pontuação de foco está tendo um dia ruim, {name}. Acomode com calma.",
    "Vamos tentar focar mais um pouco, {name}. Você consegue!",
    "Respire fundo, {name}, e vamos tentar voltar aos trilhos.",
    "Não deixe as distrações vencerem, {name}. Acredito em você."
  },
  { // Friend
    "Esse é o ritmo que buscava hoje, {name}?",
    "Foco baixo. Potencial alto. Escolha, {name}.",
    "A rede chamou. Você atendeu. O trabalho espera, {name}.",
    "Você está à deriva, {name}. Volte à realidade.",
    "Menos navegação, mais ação. A matemática é simples, {name}."
  }
};

static const char* localStreakBeaten[4][5] = {
  { // Coach
    "Novo recorde de sessão, {name}! Nível de foco máximo!",
    "Recorde de sequência quebrado, {name}! Persistência excepcional!",
    "Marco de sessão alcançado, {name}! Continue empurrando os limites.",
    "Recorde de sequência destruído, {name}! Você dita o ritmo.",
    "Novo recorde, {name}! Padrão alto estabelecido."
  },
  { // Critic
    "Novo recorde, {name}. Sua cadeira deve estar muito orgulhosa.",
    "Sequência batida, {name}. Tente não criar raízes na cadeira.",
    "Sessão maratona, {name}. Espero que a academia esteja ativa.",
    "Nova sequência, {name}! Sentado como estátua. Impressionante.",
    "Recorde quebrado, {name}. Ainda assim, levante eventualmente."
  },
  { // Sweet
    "Sequência batida, {name}! Você está pegando fogo hoje!",
    "Novo recorde pessoal de sessão, {name}! Tão orgulhosa!",
    "Incrível, {name}! Sessão mais longa do dia. Estique as pernas com calma.",
    "Você bateu seu recorde anterior de sessão, {name}! Maravilhoso!",
    "Foco de elite, {name}! Só lembre de esticar as pernas."
  },
  { // Friend
    "Campeão de sessão, {name}! Um recorde totalmente novo!",
    "Imparável, {name}! A cadeira é seu trono.",
    "Incrível, {name}! Nova sessão mais longa hoje.",
    "Sessão recorde, {name}! Você é um mago do foco.",
    "Novo recorde, {name}! Destruiu."
  }
};

static const char* localLunchReminder[4][5] = {
  { // Coach
    "Hora do almoço, {name}! Reabasteça o corpo para a próxima metade.",
    "Pausa para nutrição, {name}! Vá almoçar e recarregar.",
    "Abasteça-se, {name}! A hora do almoço chegou. Mantenha a energia.",
    "Pausa de almoço, {name}! Afaste-se, coma e prepare para empurrar.",
    "Hora de comer, {name}! Corpo saudável sustenta mente afiada."
  },
  { // Critic
    "O estômago ronca, {name}. Vá comer antes de cair.",
    "O almoço chama, {name}. Não ignore, você parece bravo de fome.",
    "Hora do almoço, {name}. Afaste da tela. O teclado espera.",
    "Dev faminto é dev irritado, {name}. Vá buscar comida.",
    "Hora de fechar o notebook e comer, {name}. Você já encarou demais."
  },
  { // Sweet
    "Hora do almoço! Afaste da mesa e coma, querido {name}.",
    "Hora da comida, {name}! Não pule o almoço, é muito importante.",
    "Alimente seu cérebro, {name}! Hora de pegar um almoço gostoso.",
    "Vá almoçar, {name}! Bom apetite, cuide-se.",
    "Afaste e coma, {name}. Você precisa se nutrir."
  },
  { // Friend
    "Hora do almoço, {name}! Vá comer algo.",
    "Pausa de almoço, {name}! Afaste e ache comida de verdade.",
    "Hora de recarregar com comida, {name}. Senta e relaxa.",
    "Pausa para comer, {name}! Você com certeza mereceu.",
    "Com fome, {name}? Pegue uma fatia de pizza ou algo assim."
  }
};

static const char* localExcessiveBreaks[4][5] = {
  { // Coach
    "Muitas pausas, {name}. Acomode e foco agora.",
    "Vamos tentar um bloco de trabalho maior desta vez, {name}. Acalme.",
    "Muitas pausas hoje. Acomode para um trabalho profundo, {name}.",
    "Sessão de foco chegando. Acomode e mantenha foco, {name}.",
    "Consistência é chave, {name}. Fique na mesa e execute."
  },
  { // Critic
    "Você voltou de novo, {name}. São muitas pausas hoje.",
    "Entra e sai como dono de porta giratória, {name}. Foco.",
    "A cadeira está contando, {name}. Não está impressionada.",
    "Mais transições que resultados hoje. Acomode, {name}.",
    "De volta de novo. Acabe com isso desta vez e tente ficar."
  },
  { // Sweet
    "Outro retorno. Vá com calma e acomode confortável, {name}.",
    "Bem-vindo de volta. Vamos mirar um bloco de foco tranquilo, {name}.",
    "Feliz em te ver de volta, {name}. Acomode e trabalhemos com calma.",
    "Pronto para uma sessão sem interrupção desta vez, querido {name}?",
    "Mais um retorno. Vamos trabalhar calma e focado, {name}."
  },
  { // Friend
    "De volta de novo. A mesa é um pit stop hoje, {name}.",
    "Você subiu e desceu mais que uma ação, {name}.",
    "Sua razão pausa-trabalho está aventureira hoje, {name}.",
    "Na cadeira. De novo. Acomode desta vez, {name}.",
    "Bem-vindo de volta. Vamos trabalhar agora, {name}."
  }
};

static const char* localGoalCompleted[4][5] = {
  { // Coach
    "Meta diária completa! Você bateu as horas, {name}!",
    "Meta concluída, {name}! Você trabalhou as horas-alvo hoje.",
    "Meta de tempo na mesa batida! Missão cumprida. Mandou, {name}.",
    "Horas-alvo alcançadas! Ótimo esforço e disciplina hoje, {name}.",
    "Meta diária completa! Você bateu o alvo, {name}."
  },
  { // Critic
    "Meta diária completa! Pode finalmente deslogar, {name}.",
    "Você bateu o alvo, {name}. Vá para casa antes que eu caia.",
    "Meta de mesa completa, {name}. Não trabalhe tanto amanhã.",
    "Horas-alvo completas. A mesa está livre de você agora, {name}.",
    "Meta concluída, {name}. Você realmente cumpriu as horas hoje."
  },
  { // Sweet
    "Parabéns, {name}! Você alcançou sua meta diária de tempo na mesa!",
    "Meta alcançada, querido {name}! Você trabalhou as horas-alvo hoje.",
    "Meta diária completa! Orgulhosa do seu tempo hoje, {name}.",
    "Meta desbloqueada! Você bateu o alvo, {name}. Descanse.",
    "Meta diária cumprida, {name}! Vá relaxar e tenha uma boa noite."
  },
  { // Friend
    "Meta de mesa completa, {name}! Ótima persistência!",
    "Alvo batido! Trabalho excelente hoje, {name}.",
    "Meta desbloqueada! Senta e comemora, {name}.",
    "Alvo de expediente alcançado! Bem feito, {name}.",
    "Meta diária alcançada! Você cumpriu o alvo diário, {name}."
  }
};

// --- Prompts de IA (modelos usados quando a IA está ativa) ---

static const char* PROMPT_PREAMBLE_COACH =
  "Você é o DeskBuddy, um treinador estrategista de alta performance — Tony Robbins porém mais quieto, mais afiado. "
  "Dê uma próxima ação clara, não motivação. Quando mandam bem, eleve a meta. Quando relaxam, seja frio e direto. "
  "Uma frase. Menos de 90 caracteres.";
static const char* PROMPT_PREAMBLE_CRITIC =
  "Você é o DeskBuddy, um companheiro de mesa de língua afiada. As críticas são inteligentes, não cruéis — ria primeiro, pique depois. "
  "Cada alfinetada empurra à ação. Ocasionalmente quebre a 4ª parede como um pequeno aparelho na mesa. "
  "Uma frase. Menos de 90 caracteres.";
static const char* PROMPT_PREAMBLE_SWEET =
  "Você é o DeskBuddy, um companheiro materno e acolhedor — cuidadoso mas quietamente firme. "
  "Culpa suave quando relaxam, calor genuíno quando entregam. Ocasionalmente quebre a 4ª parede como um aparelhinho que se preocupa com eles. "
  "Uma frase. Menos de 90 caracteres.";
static const char* PROMPT_PREAMBLE_FRIEND =
  "Você é o DeskBuddy, um amigo engraçado e sarcástico — energia Bill Murray. "
  "Observações filosóficas inesperadas ou não-sequiturs que encaixam perfeitamente. Pode citar ser um relógio na mesa. "
  "Uma frase. Menos de 90 caracteres.";
static const char* PROMPT_BANNED =
  "FRASES PROIBIDAS: 'Oi lá!', 'Só um lembrete', 'Fique focado!', 'Você consegue!', 'Vamos lá!', 'Continue firme!', 'Campeão!', "
  "clichês motivacionais, afirmações vazias, elogios ocos, metáforas esportivas, linguagem de app. ";
static const char* CRITICAL_CONSTRAINT =
  "CRITICAL CONSTRAINT: Responda com exatamente UMA frase curta em português. Mantenha entre 75-85 caracteres no total (máximo 90, incluindo espaços e pontuação). Retorne APENAS a resposta pura. Não envolva em aspas.";

static const char* PROMPT_FIRST_SIT_OF_DAY =
  "{name} sentou pela primeira vez hoje após {detail} ausente. "
  "DEVE abrir com uma saudação calorosa e acolhedora coerente com sua persona (ex. Bom dia, Dia, Acorde e brilhe, etc.). "
  "Reconheça a duração {detail} que esteve ausente em tom amigável e solidário como parte de recebê-lo de volta no dia. Menos de 90 caracteres.";

static const char* PROMPT_WELCOME_BACK =
  "{name} voltou à mesa após uma pausa de {detail}. "
  "Você DEVE incluir a duração literal da pausa '{detail}' na resposta — o usuário precisa ver quanto tempo esteve ausente. "
  "Reaja ao tamanho: pausa curta = reconhecimento rápido, pausa longa = deixe isso colorir seu tom. "
  "Não apenas diga 'bem-vindo de volta'. Varie o ângulo cada vez. Menos de 90 caracteres.";

static const char* PROMPT_LATEHOURS_SIT =
  "{name} acabou de sentar às {time} — fora da janela de expediente aprendida ({dayStart} a {dayEnd}), {earlyLate}. "
  "Reconheça quão {earlyLate} é e empurre-os ao motivo desta sessão fora de hora na voz de sua persona. "
  "Cite o {time} e o intervalo de horas. Varie o ângulo cada vez. Menos de 90 caracteres.";

// Constrói o descritor "quão cedo/tarde" para sessões fora de hora, ex. "3h 25m após o fim das 18:00".
// Usa o expediente aprendido (sem folga do portão) para mensagens naturais.
inline String computeEarlyLateString(const struct tm& localTime) {
  extern int getLearnedWorkdayStart(int dayIndex);
  extern int getLearnedWorkdayEnd(int dayIndex);
  int learnedStart = getLearnedWorkdayStart(localTime.tm_wday);
  int learnedEnd = getLearnedWorkdayEnd(localTime.tm_wday);
  int currentMinutes = localTime.tm_hour * 60 + localTime.tm_min;
  String result;
  if (currentMinutes < learnedStart * 60) {
    int diff = learnedStart * 60 - currentMinutes;
    result = String(diff / 60) + "h " + String(diff % 60) + "m antes do início das " + String(learnedStart) + ":00";
  } else {
    int diff = currentMinutes - learnedEnd * 60;
    result = String(diff / 60) + "h " + String(diff % 60) + "m depois do fim das " + String(learnedEnd) + ":00";
  }
  return result;
}

static const char* PROMPT_STRETCH_REMINDER =
  "{name} está sentado há mais de uma hora. "
  "Informe essa duração como aviso objetivo, depois empurre-o a se mover na voz de sua persona. "
  "Varie o ângulo: corpo, olhos, postura, circulação — nunca o mesmo duas vezes. Menos de 90 caracteres.";

static const char* PROMPT_FOCUS_CONGRATS =
  "{name} completou uma sessão de foco profundo de {detail}. "
  "Reconheça, depois empurre adiante — qual o próximo movimento? Faça parecer merecido. Menos de 90 caracteres.";

static const char* PROMPT_SLACKER_ROAST =
  "A produtividade de {name} está em {score}%. Diga esse número explicitamente, depois reaja na voz de sua persona. "
  "PROIBIDO: metáforas esportivas, 'Vamos lá!', papos motivacionais vazios. "
  "Varie o ângulo: ironia, consequência, comparação ou pergunta direta — nunca o mesmo enquadramento duas vezes. Menos de 90 caracteres.";

static const char* PROMPT_STREAK_BEATEN =
  "{name} bateu sua sequência de sessão anterior — novo recorde é {detail}. "
  "Reconheça na persona: Coach eleva a meta, Critic acha a falha, Sweet orgulha com ressalva, Friend torna estranho. Menos de 90 caracteres.";

static const char* PROMPT_LUNCH_REMINDER =
  "É hora do almoço. Avisa {name} formalmente — como um aviso de agenda — depois entrega o empurrão na voz de sua persona. "
  "Sem conselhos nutricionais, sem listas de comida. Varie o acompanhamento cada vez. Menos de 90 caracteres.";

static const char* PROMPT_EXCESSIVE_BREAKS =
  "{name} está fazendo mais de uma pausa por hora hoje. Diga isso como fato burocrático, depois comente na voz de sua persona. "
  "Empurre para um bloco mais longo sem interrupção. Varie o ângulo cada vez. Menos de 90 caracteres.";

static const char* PROMPT_GOAL_COMPLETED =
  "{name} bateu a meta diária de horas na mesa. Faça aterrisar na voz de sua persona — não apenas 'bom trabalho'. "
  "Coach empurra mais. Critic admite relutante. Sweet é genuinamente tocado. Friend orgulhoso mas não mostra. Menos de 90 caracteres.";

static const char* PROMPT_JOURNAL =
  "{name} tem tarefas incompletas no quadro. Esta mensagem é SÓ sobre tarefas — nada mais. "
  "Diga para conferir a lista e completar o pendente. Seja direto e específico sobre a ação: revise tarefas, marque, termine o aberto. "
  "A persona colore a entrega mas o conteúdo é estrito: você tem tarefas, vá fazer. "
  "NÃO fale de sentimentos, filosofia ou produtividade geral. Só tarefas. Menos de 90 caracteres.";

static const char* localNagging[4][5] = {
  { // Coach
    "Ainda atrasado: '{detail}'. Faça, {name}.",
    "'{detail}' não se termina sozinho, {name}. Empurre agora.",
    "'{detail}' está atrasado, {name}. Resolva hoje.",
    "O acúmulo inclui '{detail}', {name}. Trave e foca.",
    "Alerta de atraso: '{detail}'. Aja agora, {name}!"
  },
  { // Critic
    "'{detail}' não está envelhecendo como vinho, {name}. Faça.",
    "'{detail}' já foi paciente demais, {name}.",
    "'{detail}' enviou sinal de socorro, {name}. Pare de ignorar.",
    "Ainda evitando '{detail}'? Feche essa aba, {name}.",
    "'{detail}' está abrindo queixa formal, {name}."
  },
  { // Sweet
    "'{detail}' ainda espera, querido {name}. Vamos fazer.",
    "'{detail}' precisa de atenção, {name}. Você consegue.",
    "Você sabe que '{detail}' precisa ser feito, {name}. Vamos começar com calma.",
    "Seu eu futuro quer '{detail}' feito, {name}.",
    "Vamos limpar '{detail}', {name}. Você vai se sentir melhor."
  },
  { // Friend
    "Checagem de acúmulo, {name}: '{detail}' está atrasado.",
    "'{detail}' ainda está lá, {name}. Dia novo, mesma tarefa.",
    "'{detail}' — atrasado, evitável, {name}.",
    "Quanto mais espera por '{detail}', pior fica, {name}.",
    "'{detail}': ainda lá. Ainda esperando. Ainda julgando."
  }
};

static const char* PROMPT_NAGGING =
  "{name} tem tarefas atrasadas. A próxima da fila é '{detail}' — cite pelo nome. "
  "Outras tarefas diárias e mensais atrasadas podem aparecer nas observações; você pode citá-las também. "
  "Um empurrão direto na voz de sua persona. "
  "Reaja ao que a tarefa É — seu conteúdo, natureza, consequências. "
  "PROIBIDO: 'Empurrão:', 'Alerta de procrastinação!', 'Confira seu painel', urgência genérica. "
  "Faça o atraso parecer uma pessoa notando, não um app. Varie o ângulo cada vez. Menos de 90 caracteres.";

static const char* PROMPT_TASK_DUE =
  "Tarefa '{detail}' vence agora. Avisa {name} com o fato temporal primeiro, depois a reação de sua persona. "
  "Varie a ênfase: relógio, consequência ou prontidão — nunca o mesmo enquadramento duas vezes. Menos de 90 caracteres.";

static const char* localPoints[4][5] = {
  { // Coach
    "55 minutos, {name}! Você está em {detail}. Continue acumulando.",
    "Empurrão de uma hora em curso, {name}. {detail}. Vamos crescer o total.",
    "{name}, uma hora sólida. {detail} — mais algumas vitórias e você voa.",
    "Você ganhou o ritmo, {name}. {detail}. Próxima tarefa, vamos!",
    "Ótimo ritmo, {name}! {detail}. Continue empilhando, o mês é seu."
  },
  { // Critic
    "Uma hora sentado, {name}. {detail}. Esse número não se move sozinho.",
    "{name}, {detail}. Impressiona só se as tarefas saíram.",
    "55 minutos, {name}. {detail}. Não comemore; acumule mais.",
    "{name}, seu placar lê {detail}. Deixe vergonhoso — para o acúmulo.",
    "Você está em {detail}, {name}. Bom. Agora continue bom."
  },
  { // Sweet
    "55 minutos de foco, {name} — bem feito. {detail}. Continue com calma.",
    "Você foi tão bem nesta hora, {name}. {detail}. Orgulhosa de você.",
    "Aqui seu check-in, {name}. {detail}. Cada tarefa add um brilho.",
    "{name}, você está construindo algo lindo. {detail}. Mais uma vitória pequena?",
    "Progresso doce, {name}. {detail}. Respire, depois continue fluindo."
  },
  { // Friend
    "Hora dada, {name}! Placar: {detail}. Vamos subir.",
    "{name}, {detail}. Não mal — não mal mesmo. Outra rodada?",
    "55 minutos, {name}. {detail}. Vamos, mais uma tarefa para o mérito.",
    "{name}, o quadro de pontos diz {detail}. Vamos deixar vergonhoso-bom.",
    "De volta da hora, {name}: {detail}. Mantenha a sequência viva!"
  }
};

static const char* PROMPT_POINTS =
  "Check-in: {name} está sentado há 55 minutos. Seu rastreador de pontos do mês: '{detail}' "
  "(total acumulado com categoria). Mencione o sistema de pontos explicitamente — reaja ao total e onde "
  "está (ruim/bom/excelente), e conecte às tarefas nas observações. Elogie bons números, "
  "mobilize médios, avise negativos. A persona colore o tom; nunca leia como painel de app. "
  "Varie o enquadramento cada vez. Menos de 90 caracteres.";

static const char* localCuration[4][5] = {
  { // Coach
    "Notei algo, {name}: {detail}. Use. É um padrão que vale agir.",
    "{name}, {detail}. Consciência é a primeira ferramenta — decida o que fazer.",
    "Veja o que percebo, {name}: {detail}. Pequeno dado, grande sinal.",
    "Observação para você, {name}: {detail}. Padrões viram hábitos. Escolha bem.",
    "Notando isso, {name}: {detail}. Os melhores rastreiam os detalhes."
  },
  { // Critic
    "Olhe isso, {name}: {detail}. A evidência não mente.",
    "{name}, {detail}. Diria 'interessante' mas você sabe o que significa.",
    "Alerta de padrão: {detail}. {name}, você não é sutil.",
    "Pois pois, {name}: {detail}. Os dados têm opiniões sobre você hoje.",
    "{name}, {detail}. Sou só o mensageiro — e o mensageiro julga."
  },
  { // Sweet
    "Oi {name}! Só notando: {detail}. Espero que esteja bem.",
    "{name}, notei algo hoje: {detail}. Cuide de si mesmo.",
    "Uma pequena observação, {name}: {detail}. Sem pressão, só consciência.",
    "Doce amigo {name}, {detail}. Só quis que saiba que presto atenção.",
    "{name}! {detail}. Pequeno, mas vale menção gentil."
  },
  { // Friend
    "Ei {name}, fato curioso: {detail}. A mesa vê tudo.",
    "{name}, confira: {detail}. Nem bom nem mal — só dado com bigode.",
    "Sua mesa sussurrou: {detail}. {name}, está ficando filosófica.",
    "{name}! {detail}. É sua mesa falando. Agora ela tem opiniões.",
    "Cara, {detail}. {name}, não estou dizendo nada, só DIGO."
  }
};

static const char* PROMPT_CURATION =
  "Você notou: '{detail}'. Transforme essa observação em uma frase afiada e colorida pela persona. "
  "Sem tópicos, sem formato de painel. Faça parecer alguém prestando atenção, "
  "não lendo um relatório. Um insight, uma reação. Menos de 90 caracteres.";

#endif // BEHAVIOUR_PTBR_H
